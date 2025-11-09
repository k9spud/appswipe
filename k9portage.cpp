// Copyright (c) 2021-2025, K9spud LLC.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "k9portage.h"
#include "datastorage.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

K9Portage::K9Portage(QObject *parent) : QObject(parent)
{
    separator.setPattern("([^A-Za-z0-9]+)");
    digitVersion.setPattern("([0-9]+)");
    alphaVersion.setPattern("([A-Za-z]+)");

    // see https://devmanual.gentoo.org/general-concepts/dependencies/
    // See Restrictions upon Names: https://projects.gentoo.org/pms/8/pms.html#x1-150003
    //                              https://dev.gentoo.org/~ulm/pms/head/pms.html#chapter-3
    dependKeywordsBasicRE.setPattern("([^~=><][^/]*)/([^:\\n\\s]+)\\s+(.+)$");
    dependKeywordsVersionRE.setPattern("(~|=|>=|<=|>|<)([^/]+)/([^:]+)-([0-9\\*][0-9\\-\\.A-z\\*]*)\\s+(.+)$");
    dependKeywordsSlotRE.setPattern("([^~=><][^/]*)/([^:\\n]+):([^:\\n\\s]+)\\s+(.+)$");
    dependKeywordsRepositoryRE.setPattern("(~|=|>=|<=|>|<)([^/]+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n\\s]+)\\s+(.+)$");

/*
To specify "version 2.x (not 1.x or 3.x)" of a package, it is necessary to use the asterisk postfix like this: "=x11-libs/gtk+-2*"
how come there is no dot before the asterisk? wouldn't this allow version 21.x as well as 2.x?
<ionen> K9spud: * matches on a full version component in this context, e.g. it could patch 2_alpha, 2.1.3 but not 21
ionen> 2.* would've been inconvenient for components that start with _
<sam_> K9spud: read PMS 8.3.1 for the definition of what =* does

https://dev.gentoo.org/~ulm/pms/head/pms.html#section-8.3.1

8.3.1 Operators

The following operators are available:

<   Strictly less than the specified version.

<=  Less than or equal to the specified version.

=   Exactly equal to the specified version.
    Special exception: if the version specified has an asterisk immediately following it,
                       then only the given number of version components is used for comparison,
                       i. e. the asterisk acts as a wildcard for any further components.
    When an asterisk is used, the specification must remain valid if the asterisk were removed.
    (An asterisk used with any other operator is illegal.)

~   Equal to the specified version when revision parts are ignored.

>=  Greater than or equal to the specified version.

>   Strictly greater than the specified version.

*/


#if defined(__x86_64__)
    arch = "amd64";
#elif defined(__i386__)
    arch = "x86";
#elif defined(__aarch64__)
    arch = "arm64";
#elif defined(__arm__)
    arch = "arm";
#elif defined(__mips__)
    arch = "mips";
#elif defined(__ppc64__)
    arch = "ppc64";
#elif defined(__powerpc__)
    arch = "ppc";
#elif defined(__sparc__)
    arch = "sparc";
#elif defined(__m68k__)
    arch = "m68k";
#elif defined(__alpha__)
    arch = "alpha";
#elif defined(__hppa__)
    arch = "hppa";
#elif defined(__ia64__)
    arch = "ia64";
#elif defined(__riscv__)
    arch = "riscv";
#elif defined(__s390__)
    arch = "s390";
#endif

}

void K9Portage::autoKeyword(QString app, QString op)
{
    QString keywords;
    QFile file("/etc/portage/package.accept_keywords/appswipe.tmp");
    if(file.exists() == false || file.open(QIODevice::Text | QIODevice::Append | QIODevice::WriteOnly) == false)
    {
        return;
    }

    QSqlDatabase db;
    if(QSqlDatabase::contains("GuiThread") == false)
    {
        db = QSqlDatabase::addDatabase("QSQLITE", "GuiThread");
    }
    else
    {
        db = QSqlDatabase::database("GuiThread");
        if(db.isValid())
        {
            db.close();
        }
    }

    db.setDatabaseName(ds->storageFolder + ds->databaseFileName);
    db.open();
    QSqlQuery query(db);
    QString s = R"EOF(
select p.KEYWORDS, p.MASKED, r.REPO, c.CATEGORY, p.PACKAGE, p.VERSION, p.SLOT, p.SUBSLOT
from PACKAGE p
inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID
inner join REPO r on r.REPOID = p.REPOID
where c.CATEGORY || '/' || p.PACKAGE || '-' || p.VERSION = ?
order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc, p.V7 desc, p.V8 desc, p.V9 desc, p.V10 desc
)EOF";

    query.prepare(s);
    query.bindValue(0, app);
    if(query.exec() && query.first())
    {
        keywords = query.value(0).toString();
        int masked = query.value(1).toInt();
        QString repo = query.value(2).toString();
        QString category = query.value(3).toString();
        QString package = query.value(4).toString();
        QString version = query.value(5).toString();
        QString slot = query.value(6).toString();
        QString subslot = query.value(7).toString();
        if(masked == 0 && keywords.contains(arch) && keywords.contains(QString("~%1").arg(arch)) == false)
        {
            // this package doesn't need to be auto keyworded
            return;
        }

        QDir dir;
        dir.setPath("/etc/portage/package.accept_keywords");
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        foreach(QString keywordFile, dir.entryList())
        {
            if(foundKeyworded(QString("/etc/portage/package.accept_keywords/%1").arg(keywordFile), category, package, version, repo, slot, subslot, keywords))
            {
                // this package has already been keyworded before
                return;
            }
        }
    }

    QTextStream out(&file);
    if(op.isEmpty())
    {
        op = "=";
    }

    if(keywords.contains(QString("~%1").arg(arch)))
    {
        out << QString("%1%2 ~%3").arg(op, app, arch) << Qt::endl;
    }
    else
    {
        out << QString("%1%2 **").arg(op, app) << Qt::endl;
    }

    file.close();
}

bool K9Portage::globMatch(QString glob, QString s)
{
    if(glob == "*")
    {
        return true;
    }

    if(glob.contains('*') == false)
    {
        return (glob == s);
    }

    QString globregex = QRegularExpression::wildcardToRegularExpression(glob);
    QRegularExpression regex;
    regex.setPattern(globregex);
    return regex.match(s).hasMatch();
}

bool K9Portage::keywordMatch(QString acceptKeywords, QString appKeywords)
{
    QString s;
    /*
    See https://wiki.gentoo.org/wiki/ACCEPT_KEYWORDS

        ~amd64 - package is visisble if ~amd64 testing keyword is present, even if you're running on arm64
        ~amd64 ~arm64 - package is only visible if both ~amd64 and ~arm64 testing keywords are present in the ebuild

    See https://wiki.gentoo.org/wiki/Knowledge_Base:Accepting_a_keyword_for_a_single_package
        sounds like the keyword itself can be omitted. this makes portage install the testing version.
    */
    if(acceptKeywords.isEmpty())
    {
        if(appKeywords.contains(QString("~%1").arg(arch)))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    QStringList keywordsList = acceptKeywords.remove("\n").split(' ');
    QStringList appKeywordsList = appKeywords.remove("\n").split(' ');
    QStringList keys;
    foreach(s, keywordsList)
    {
        if(s == "**") // Package is always visible (KEYWORDS are ignored completely). even 9999 live ebuilds now get included
        {
            return true;
        }
        else if(s == "*") // Package is visible if it is stable on any architecture.
        {
            foreach(s, appKeywordsList)
            {
                if(s.size() > 0 && s.startsWith('~') == false)
                {
                    return true;
                }
            }
        }
        else if(s == "~*") // Package is visible if it is in testing on any architecture.
        {
            foreach(s, appKeywordsList)
            {
                if(s.startsWith('~') == true)
                {
                    return true;
                }
            }
        }
        else if(s.size())
        {
            keys.append(s);
        }
    }

    foreach(s, keys)
    {
        if(appKeywordsList.contains(s) == false)
        {
            return false;
        }
    }

    return false;
}

bool K9Portage::foundKeyworded(QString fileName, QString appCategory, QString appPackage, QString appVersion, QString appRepo, QString appSlot, QString appSubslot, QString appKeywords)
{
    QFile input;
    input.setFileName(fileName);
    if(!input.exists() || !input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QString wholeFile = input.readAll();
    input.close();
    QStringList lines = wholeFile.split('\n');

    QRegularExpressionMatch match;
    VersionString vs;
    QString s;
    QString filter;
    QString category;
    QString package;
    QString version;
    VersionString appVerString;
    appVerString.parse(appVersion);
    QString repo;
    QString slot;
    QString subslot;
    QString keywords;

    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        match = dependKeywordsVersionRE.match(s);
        if(match.hasMatch())
        {
            filter = match.captured(1);
            category = match.captured(2);
            package = match.captured(3);
            version = match.captured(4);
            keywords = match.captured(5).trimmed();

            if(category == appCategory && package == appPackage && appVerString.match(filter, version))
            {
                if(keywordMatch(keywords, appKeywords))
                {
                    return true;
                }
            }
            continue;
        }

        match = dependKeywordsBasicRE.match(s);
        if(match.hasMatch())
        {
            category = match.captured(1);
            package = match.captured(2);
            keywords = match.captured(3).trimmed();

            if(globMatch(category, appCategory) && globMatch(package, appPackage))
            {
                if(keywordMatch(keywords, appKeywords))
                {
                    return true;
                }
            }
            continue;
        }

        match = dependKeywordsSlotRE.match(s);
        if(match.hasMatch())
        {
            category = match.captured(1);
            package = match.captured(2);
            slot = match.captured(3);
            keywords = match.captured(4).trimmed();
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot = "*";
            }

            if(globMatch(category, appCategory) && globMatch(package, appPackage) &&
               globMatch(slot, appSlot) && globMatch(subslot, appSubslot))
            {
                if(keywordMatch(keywords, appKeywords))
                {
                    return true;
                }
            }
            continue;
        }

        match = dependKeywordsRepositoryRE.match(s);
        if(match.hasMatch())
        {
            filter = match.captured(1);
            category = match.captured(2);
            package = match.captured(3);
            version = match.captured(4);
            slot = match.captured(5);
            keywords = match.captured(6).trimmed();
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot = "*";
            }

            repo = match.captured(6);

            if(globMatch(category, appCategory) && globMatch(package, appPackage) &&
               appVerString.match(filter, version) && (repo.isEmpty() || globMatch(repo, appRepo)) &&
               globMatch(slot, appSlot) && globMatch(subslot, appSubslot))
            {
                if(keywordMatch(keywords, appKeywords))
                {
                    return true;
                }
            }
            continue;
        }

        qDebug() << "Could not parse depend string:";
        qDebug() << "  " << s;
        continue;
    }

    return false;
}


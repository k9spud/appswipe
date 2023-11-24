// Copyright (c) 2021-2023, K9spud LLC.
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
#include "globals.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QRegularExpressionMatch>

K9Portage::K9Portage(QObject *parent) : QObject(parent)
{
    separator.setPattern("([^A-Za-z0-9]+)");
    digitVersion.setPattern("([0-9]+)");
    alphaVersion.setPattern("([A-Za-z]+)");

    stringAssignment.setPattern("([^=]+)\\s*=\\s*\"((\\\\\"|[^\"])*)\"");
    variableAssignment.setPattern("([^=]+)\\s*=\\s*([^\"]*)");
    verCutSingle.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)\\)");
    verCutRange.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)-([0-9]+)\\)");
    var_ref.setPattern("\\$\\{([A-z, 0-9, _]+)\\}");

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

void K9Portage::setRepoFolder(QString path)
{
    repoFolder = path;
    if(repoFolder.isEmpty())
    {
        repoFolder = "/var/db/repos/";
    }
    else if (repoFolder.endsWith('/') == false)
    {
        repoFolder.append('/');
    }

    QDir dir;
    dir.setPath(repoFolder);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    repos.clear();
    foreach(QString repoDir, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        repos.append(repoFolder + repoDir + "/");
    }

    categories.clear();
    foreach(QString repo, repos)
    {
        dir.setPath(repo);
        dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach(QString cat, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            if(categories.contains(cat) == false)
            {
                categories.append(cat);
            }
        }
    }
}

// see https://devmanual.gentoo.org/ebuild-writing/variables/ for a list of all predefined read-only variables in an ebuild
// portageq envvar might be useful to retrieve more variables
// portageq envvar CFLAGS
void K9Portage::setVersion(QString v)
{
    version.parse(v);
    vars["PVR"] = version.pvr;
    vars["PV"] = version.pv();
    vars["PR"] = version.pr();
    vars["P"] = QString("%1-%2").arg(vars["PN"], vars["PV"]);
    vars["PF"] = QString("%1-%2").arg(vars["PN"], version.pvr);
}

QVariant K9Portage::var(QString key)
{
    if(vars.contains(key))
    {
        return vars[key];
    }

    return QVariant(QVariant::String);
}

void K9Portage::parseVerCut(QString& value)
{
    bool matchFound;
    QRegularExpressionMatch match;
    int i, j;
    do
    {
        matchFound = false;
        match = verCutRange.match(value);
        if(match.hasMatch())
        {
            matchFound = true;
            i = match.captured(2).toInt() - 1;
            j = match.captured(3).toInt() - 1;
            value.replace(match.capturedStart(), match.capturedLength(), QString("%1.%2").arg(version.cut(i), version.cut(j)));
        }

        match = verCutSingle.match(value);
        if(match.hasMatch())
        {
            matchFound = true;
            i = match.captured(2).toInt() - 1;
            value.replace(match.capturedStart(), match.capturedLength(), version.cut(i));
        }
    } while(matchFound);
}

void K9Portage::emergedApp(QString app)
{
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

    QStringList atoms;
    if(app.contains(' '))
    {
        atoms = app.split(' ');
    }
    else
    {
        atoms.append(app);
    }

    QSqlQuery query(db);
    QSqlQuery updatePackage(db);
    QSqlQuery deletePackage(db);
    query.prepare("select PACKAGEID, VERSION, INSTALLED, OBSOLETED, DOWNLOADSIZE from PACKAGE where CATEGORYID=(select CATEGORYID from CATEGORY where CATEGORY=?) and PACKAGE=?");
    updatePackage.prepare("update PACKAGE set INSTALLED=?, DOWNLOADSIZE=? where PACKAGEID=?");
    deletePackage.prepare("delete from PACKAGE where PACKAGEID=?");

    QStringList x;
    QString category;
    QString packageName;
    QFile input;
    QString data;
    bool ok;
    foreach(QString atom, atoms)
    {
        x = atom.split('/');
        category = x.first();
        packageName = x.last();
        query.bindValue(0, category);
        query.bindValue(1, packageName);

        if(query.exec() == false || query.first() == false)
        {
            return;
        }

        qint64 packageId;
        qint64 installed;
        QString version;
        QString installedFilePath;
        int downloadSize;
        QFileInfo fi;
        bool obsoleted;

        do
        {
            packageId = query.value(0).toInt();
            version = query.value(1).toString();
            installed = query.value(2).toInt();
            obsoleted = query.value(3).toInt() != 0;
            downloadSize = query.value(4).toInt();

            if(downloadSize == -1)
            {
                data = QString("/var/db/pkg/%1/%2-%3/SIZE").arg(category, packageName, version);
                input.setFileName(data);
                if(input.open(QIODevice::ReadOnly))
                {
                    data = input.readAll();
                    input.close();
                    data = data.trimmed();
                    downloadSize = data.toInt(&ok);
                    if(ok == false)
                    {
                        downloadSize = -1;
                    }
                }
            }

            installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(category, packageName, version);
            fi.setFile(installedFilePath);
            if(fi.exists())
            {
                if(installed == 0)
                {
                    installed = fi.birthTime().toSecsSinceEpoch();
                    updatePackage.bindValue(0, installed);
                    updatePackage.bindValue(1, downloadSize);
                    updatePackage.bindValue(2, packageId);
                    if(updatePackage.exec() == false)
                    {
                        output << QString("update installed %1/%2-%3 failed").arg(category, packageName, version) << Qt::endl;
                    }
                }
            }
            else
            {
                if(installed)
                {
                    if(obsoleted)
                    {
                        deletePackage.bindValue(0, packageId);
                        if(deletePackage.exec() == false)
                        {
                            output << QString("delete uninstalled obsolete %1/%2-%3 failed").arg(category, packageName, version) << Qt::endl;
                        }
                    }
                    else
                    {
                        installed = 0;
                        updatePackage.bindValue(0, installed);
                        updatePackage.bindValue(1, downloadSize);
                        updatePackage.bindValue(2, packageId);
                        if(updatePackage.exec() == false)
                        {
                            output << QString("update uninstalled %1/%2-%3 failed").arg(category, packageName, version) << Qt::endl;
                        }
                    }
                }
            }
        } while(query.next());
    }
}

void K9Portage::ebuildReader(QString fileName)
{
    QFile input(fileName);
    if(!input.exists())
    {
        output << "eBuild " << input.fileName() << " does not exist." << Qt::endl;
        return;
    }

    if(!input.open(QIODevice::ReadOnly))
    {
        output << "eBuild " << input.fileName() << " could not be opened for reading." << Qt::endl;
        return;
    }

    QString s;
    QString key;
    QString value;
    QRegularExpressionMatch match;
    QTextStream in(&input);
    while(in.atEnd() == false)
    {
        s = in.readLine();
        s = s.trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        match = stringAssignment.match(s, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
        if(match.hasMatch() && match.lastCapturedIndex() >= 2)
        {
            key = match.captured(1).toUpper();
            value = match.captured(2).replace("\\\"", "\"");
        }
        else
        {
            match = variableAssignment.match(s, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
            if(match.hasMatch() && match.lastCapturedIndex() >= 2)
            {
                key = match.captured(1).toUpper();
                value = match.captured(2);
            }
        }

        if(key.isEmpty() == false)
        {
            parseVerCut(value);

            match = var_ref.match(value, 0, QRegularExpression::PartialPreferFirstMatch, QRegularExpression::NoMatchOption);
            while(match.hasMatch())
            {
                s = match.captured(1).toUpper();
                if(vars.contains(s))
                {
                    value.replace(match.capturedStart(), match.capturedLength(), vars[s]);
                }
                else
                {
                    value.remove(match.capturedStart(), match.capturedLength());
                }
                match = var_ref.match(value, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
            }

            vars[key] = value;
            key.clear();
        }
    }
}

void K9Portage::md5cacheReader(QString fileName)
{
    QFile input(fileName);
    if(!input.exists())
    {
        output << "md5-cache " << input.fileName() << " does not exist." << Qt::endl;
        return;
    }

    if(!input.open(QIODevice::ReadOnly))
    {
        output << "md5-cache " << input.fileName() << " could not be opened for reading." << Qt::endl;
        return;
    }

    QString s;
    QString key;
    QString value;
    QRegularExpressionMatch match;
    QTextStream in(&input);
    int i;
    while(in.atEnd() == false)
    {
        s = in.readLine();
        s = s.trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        i = s.indexOf('=');
        if(i == -1)
        {
            continue;
        }
        key = s.left(i);
        value = s.mid(i + 1);
        vars[key] = value;
    }
}

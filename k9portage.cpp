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

    stringAssignment.setPattern("([^=]+)\\s*=\\s*\"((\\\\\"|[^\"])*)\"");
    variableAssignment.setPattern("([^=]+)\\s*=\\s*([^\"]*)");
    verCutSingle.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)\\)");
    verCutRange.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)-([0-9]+)\\)");
    var_ref.setPattern("\\$\\{([A-z, 0-9, _]+)\\}");

    // see https://devmanual.gentoo.org/general-concepts/dependencies/
    dependBasicRE.setPattern("([^~=><].*)/([^:\\n]+)$");
    dependKeywordsBasicRE.setPattern("([^~=><].*)/([^:\\n\\s]+)\\s+(.+)$");
    dependVersionRE.setPattern("(~|=|>=|<=|>|<)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*)");
    dependKeywordsVersionRE.setPattern("(~|=|>=|<=|>|<)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*)\\s+(.+)$");
    dependSlotRE.setPattern("([^~=><].*)/([^:\\n]+):([^:\\n]+)");
    dependKeywordsSlotRE.setPattern("([^~=><].*)/([^:\\n]+):([^:\\n\\s]+)\\s+(.+)$");
    dependRepositoryRE.setPattern("(~|=|>=|<=|>|<)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n]+)");
    dependKeywordsRepositoryRE.setPattern("(~|=|>=|<=|>|<)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n\\s]+)\\s+(.+)$");

    dependLinkRE.setPattern("(!!>=|!!<=|!!~|!!=|!!>|!!<|!>=|!<=|!~|!=|!>|!<|!!|>=|<=|~|=|>|<|!)(.+)/(.+)-([0-9\\*][^\\n]*)");
    dependLinkSlotRE.setPattern("(!!>=|!!<=|!!~|!!=|!!>|!!<|!>=|!<=|!~|!=|!>|!<|!!|>=|<=|~|=|>|<|!|)(.+)/(.+):([^\\n]+)");
    dependLinkAppRE.setPattern("(!!|!|)(.+)/([^\\[\\]\\n]+)(\\[[^\\n]+\\]|)");

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

// refer to https://web.archive.org/web/20201112040501/https://wiki.gentoo.org/wiki/Version_specifier
// for DEPEND atom syntax
void K9Portage::readMaskFile(QSqlQuery& query, QString fileName, QString atomFilter)
{
    QFile input;
    input.setFileName(fileName);
    if(!input.exists() || !input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QString wholeFile = input.readAll();
    input.close();
    QStringList lines = wholeFile.split('\n');

    QRegularExpressionMatch match;
    QString clauses;
    VersionString vs;
    QString s;
    QString filter;
    QString category;
    QString package;
    QString version;
    QString repo;
    QString slot;
    QString subslot;

    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        if(atomFilter.isEmpty() == false && s.contains('*') == false && s.contains(atomFilter) == false)
        {
            continue;
        }

        clauses.clear();
        match = dependVersionRE.match(s);
        if(match.hasMatch())
        {
            filter = match.captured(1);
            category = match.captured(2);
            category.replace("*", "%");
            package = match.captured(3);
            package.replace("*", "%");
            version = match.captured(4);
            version.replace("*", "%");

            if(filter == "=")
            {
                clauses = equalFilter(category, package, version);
                if(clauses.isEmpty())
                {
                    qDebug() << "Empty WHERE clause for:" << s;
                    continue;
                }

                query.prepare("update PACKAGE set MASKED=1 where " + clauses);
                if(category != "%")
                {
                    query.addBindValue(category);
                }

                if(package != "%")
                {
                    query.addBindValue(package);
                }

                if(version != "%")
                {
                    query.addBindValue(version);
                }

                if(query.exec() == false)
                {
                    qDebug() << "FAIL:" << query.lastError().text();
                    qDebug() << "  " << query.executedQuery();
                }

                continue;
            }

            if(filter == "<=" || filter == ">=" || filter == "<" || filter == ">")
            {
                clauses = comparisonFilter(filter, category, package, version);

                if(clauses.isEmpty())
                {
                    qDebug() << "Empty WHERE clause for:" << s;
                    continue;
                }

                query.prepare("update PACKAGE set MASKED=1 where " + clauses);
                if(category != "%")
                {
                    query.addBindValue(category);
                }

                if(package != "%")
                {
                    query.addBindValue(package);
                }

                if(query.exec() == false)
                {
                    qDebug() << "FAIL:" << query.lastError().text();
                    qDebug() << "  " << query.executedQuery();
                }
                continue;
            }

            if(filter == "~")
            {
                clauses = anyRevisionFilter(category, package, version);
                if(clauses.isEmpty())
                {
                    qDebug() << "Empty WHERE clause for:" << s;
                    continue;
                }

                query.prepare("update PACKAGE set MASKED=1 where " + clauses);
                if(category != "%")
                {
                    query.addBindValue(category);
                }

                if(package != "%")
                {
                    query.addBindValue(package);
                }


                if(version != "%")
                {
                    query.addBindValue(version);
                    query.addBindValue(QString("%1-r%").arg(version));
                }

                if(query.exec() == false)
                {
                    qDebug() << "FAIL:" << query.lastError().text();
                    qDebug() << "  " << query.executedQuery();
                }

                continue;
            }

            qDebug() << "Unknown mask filter type:" << filter;
            qDebug() << "  " << s;
            continue;
        }

        match = dependBasicRE.match(s);
        if(match.hasMatch())
        {
            category = match.captured(1);
            category.replace("*", "%");
            package = match.captured(2);
            package.replace("*", "%");

            if(category != "%")
            {
                if(category.contains("%"))
                {
                    clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY like ?)");
                }
                else
                {
                    clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY=?)");
                }
            }

            if(package != "%")
            {
                if(clauses.isEmpty() == false)
                {
                    clauses.append(" and ");
                }

                if(package.contains("%"))
                {
                    clauses.append("PACKAGE like ?");
                }
                else
                {
                    clauses.append("PACKAGE=?");
                }
            }

            if(clauses.isEmpty())
            {
                qDebug() << "Empty WHERE clause for:" << s;
                continue;
            }

            query.prepare("update PACKAGE set MASKED=1 where " + clauses);
            if(category != "%")
            {
                query.addBindValue(category);
            }

            if(package != "%")
            {
                query.addBindValue(package);
            }

            if(query.exec() == false)
            {
                qDebug() << "FAIL:" << query.lastError().text();
                qDebug() << "  " << query.executedQuery();
            }
            continue;
        }

        match = dependSlotRE.match(s);
        if(match.hasMatch())
        {
            category = match.captured(1);
            category.replace("*", "%");
            package = match.captured(2);
            package.replace("*", "%");
            slot = match.captured(3);
            slot.replace("*", "%");
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot = "%";
            }

            if(category != "%")
            {
                if(category.contains("%"))
                {
                    clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY like ?)");
                }
                else
                {
                    clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY=?)");
                }
            }

            if(package != "%")
            {
                if(clauses.isEmpty() == false)
                {
                    clauses.append(" and ");
                }

                if(package.contains("%"))
                {
                    clauses.append("PACKAGE like ?");
                }
                else
                {
                    clauses.append("PACKAGE=?");
                }
            }

            if(slot != "%")
            {
                if(clauses.isEmpty() == false)
                {
                    clauses.append(" and ");
                }

                if(slot.contains("%"))
                {
                    clauses.append("SLOT like ?");
                }
                else
                {
                    clauses.append("SLOT=?");
                }
            }

            if(subslot != "%")
            {
                if(clauses.isEmpty() == false)
                {
                    clauses.append(" and ");
                }

                if(subslot.contains("%"))
                {
                    clauses.append("SUBSLOT like ?");
                }
                else
                {
                    clauses.append("SUBSLOT=?");
                }
            }

            if(clauses.isEmpty())
            {
                qDebug() << "Empty WHERE clause for:" << s;
                continue;
            }

            query.prepare("update PACKAGE set MASKED=1 where " + clauses);
            if(category != "%")
            {
                query.addBindValue(category);
            }

            if(package != "%")
            {
                query.addBindValue(package);
            }

            if(slot != "%")
            {
                query.addBindValue(slot);
            }

            if(subslot != "%")
            {
                query.addBindValue(subslot);
            }

            if(query.exec() == false)
            {
                qDebug() << "FAIL:" << query.lastError().text();
                qDebug() << "  " << query.executedQuery();
            }

            continue;
        }

        match = dependRepositoryRE.match(s);
        if(match.hasMatch())
        {
            filter = match.captured(1);
            category = match.captured(2);
            category.replace("*", "%");
            package = match.captured(3);
            package.replace("*", "%");
            version = match.captured(4);
            version.replace("*", "%");
            slot = match.captured(5);
            slot.replace("*", "%");
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot = "%";
            }

            repo = match.captured(6);
            repo.replace("*", "%");

            if(filter == "=")
            {
                clauses = equalFilter(category, package, version);
                if(slot != "%" && slot.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(slot.contains("%"))
                    {
                        clauses.append("SLOT like ?");
                    }
                    else
                    {
                        clauses.append("SLOT=?");
                    }
                }

                if(subslot != "%" && slot.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(subslot.contains("%"))
                    {
                        clauses.append("SUBSLOT like ?");
                    }
                    else
                    {
                        clauses.append("SUBSLOT=?");
                    }
                }

                if(repo != "%" && repo.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(repo.contains("%"))
                    {
                        clauses.append("REPOID in (select REPOID from REPO where REPO like ?)");
                    }
                    else
                    {
                        clauses.append("REPOID in (select REPOID from REPO where REPO=?)");
                    }
                }

                if(clauses.isEmpty())
                {
                    qDebug() << "Empty WHERE clause for:" << s;
                    continue;
                }

                query.prepare("update PACKAGE set MASKED=1 where " + clauses);
                if(category != "%")
                {
                    query.addBindValue(category);
                }

                if(package != "%")
                {
                    query.addBindValue(package);
                }

                if(version != "%")
                {
                    query.addBindValue(version);
                }

                if(slot != "%" && slot.isEmpty() == false)
                {
                    query.addBindValue(slot);
                }

                if(subslot != "%" && subslot.isEmpty() == false)
                {
                    query.addBindValue(subslot);
                }

                if(repo != "%" && repo.isEmpty() == false)
                {
                    query.addBindValue(repo);
                }

                if(query.exec() == false)
                {
                    qDebug() << "FAIL:" << query.lastError().text();
                    qDebug() << "  " << query.executedQuery();
                }

                continue;
            }
            else if(filter == "<=" || filter == ">=" || filter == "<" || filter == ">")
            {
                clauses = comparisonFilter(filter, category, package, version);
                if(slot != "%" && slot.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(slot.contains("%"))
                    {
                        clauses.append("SLOT like ?");
                    }
                    else
                    {
                        clauses.append("SLOT=?");
                    }
                }

                if(subslot != "%" && subslot.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(subslot.contains("%"))
                    {
                        clauses.append("SUBSLOT like ?");
                    }
                    else
                    {
                        clauses.append("SUBSLOT=?");
                    }
                }

                if(repo != "%" && repo.isEmpty() == false)
                {
                    if(clauses.isEmpty() == false)
                    {
                        clauses.append(" and ");
                    }

                    if(repo.contains("%"))
                    {
                        clauses.append("REPOID in (select REPOID from REPO where REPO like ?)");
                    }
                    else
                    {
                        clauses.append("REPOID in (select REPOID from REPO where REPO=?)");
                    }
                }

                if(clauses.isEmpty())
                {
                    qDebug() << "Empty WHERE clause for:" << s;
                    continue;
                }

                query.prepare("update PACKAGE set MASKED=1 where " + clauses);
                if(category != "%")
                {
                    query.addBindValue(category);
                }

                if(package != "%")
                {
                    query.addBindValue(package);
                }

                if(slot != "%" && slot.isEmpty() == false)
                {
                    query.addBindValue(slot);
                }

                if(subslot != "%" && subslot.isEmpty() == false)
                {
                    query.addBindValue(subslot);
                }

                if(repo != "%" && repo.isEmpty() == false)
                {
                    query.addBindValue(repo);
                    repo.clear();
                }

                if(query.exec() == false)
                {
                    qDebug() << "FAIL:" << query.lastError().text();
                    qDebug() << "  " << query.executedQuery();
                }

                continue;
            }

            qDebug() << "Unknown mask filter type (Depend Repo):" << filter;
            qDebug() << "  " << s;
            continue;
        }

        qDebug() << "Could not parse depend string:";
        qDebug() << "  " << s;
        continue;
    }
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
                        qDebug() << QString("update installed %1/%2-%3 failed").arg(category, packageName, version);
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
                            qDebug() << QString("delete uninstalled obsolete %1/%2-%3 failed").arg(category, packageName, version);
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
                            qDebug() << QString("update uninstalled %1/%2-%3 failed").arg(category, packageName, version);
                        }
                    }
                }
            }
        } while(query.next());
    }
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
                if(s.count() > 0 && s.startsWith('~') == false)
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
        else if(s.count())
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

    QRegularExpression regex;
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
    QString globregex;
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

void K9Portage::applyMasks(QSqlDatabase& db, QString atomFilter)
{
    QDir dir;

    QSqlQuery query(db);
    db.transaction();

    dir.setPath("/etc/portage/package.mask");
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    foreach(QString maskFile, dir.entryList())
    {
        readMaskFile(query, QString("%1/%2").arg(dir.path(), maskFile), atomFilter);
    }

    foreach(QString repo, repos)
    {
        readMaskFile(query, QString("%1profiles/package.mask").arg(repo), atomFilter);
    }

    db.commit();
}

QString K9Portage::linkDependency(QString input, QString& category, QString& package)
{
    QRegularExpressionMatch match = dependLinkRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString version = match.captured(4);
        filter.replace("<", "&lt;");
        filter.replace(">", "&gt;");
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>-%4").arg(filter, category, package, version);
        }
    }

    match = dependLinkSlotRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString slot = match.captured(4);
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>:%4").arg(filter, category, package, slot);
        }
    }

    match = dependLinkAppRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString uses = match.captured(4);
        filter.replace("<", "&lt;");
        filter.replace(">", "&gt;");
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>%4").arg(filter, category, package, uses);
        }
    }

    input.replace("<", "&lt;");
    input.replace(">", "&gt;");
    return input;
}

QString K9Portage::equalFilter(QString& category, QString& package, QString& version)
{
    QString clauses;

    if(category != "%")
    {
        if(category.contains("%"))
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY like ?)");
        }
        else
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY=?)");
        }
    }

    if(package != "%")
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }

        if(package.contains("%"))
        {
            clauses.append("PACKAGE like ?");
        }
        else
        {
            clauses.append("PACKAGE=?");
        }
    }

    if(version != "%")
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }

        if(version.contains("%"))
        {
            clauses.append("VERSION like ?");
        }
        else
        {
            clauses.append("VERSION=?");
        }
    }

    return clauses;
}

QString K9Portage::anyRevisionFilter(QString& category, QString& package, QString& version)
{
    QString clauses;

    if(category != "%")
    {
        if(category.contains("%"))
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY like ?)");
        }
        else
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY=?)");
        }
    }

    if(package != "%")
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }

        if(package.contains("%"))
        {
            clauses.append("PACKAGE like ?");
        }
        else
        {
            clauses.append("PACKAGE=?");
        }
    }

    if(version != "%")
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }

        if(version.contains("%"))
        {
            clauses.append("(VERSION like ? or VERSION like ?)");
        }
        else
        {
            clauses.append("(VERSION=? or VERSION like ?)");
        }
    }

    return clauses;
}

QString K9Portage::comparisonFilter(QString& filter, QString& category, QString& package, QString& version)
{
    QString clauses;
    VersionString vs;

    if(category != "%")
    {
        if(category.contains("%"))
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY like ?)");
        }
        else
        {
            clauses.append("CATEGORYID in (select CATEGORYID from CATEGORY where CATEGORY=?)");
        }
    }

    if(package != "%")
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }

        if(package.contains("%"))
        {
            clauses.append("PACKAGE like ?");
        }
        else
        {
            clauses.append("PACKAGE=?");
        }
    }

    vs.parse(version);

    if(filter == "<=")
    {
        vs.lessThanEqualToSQL(clauses);
    }

    if(filter == ">=")
    {
        vs.greaterThanEqualToSQL(clauses);
    }

    if(filter == ">")
    {
        vs.greaterThanSQL(clauses);
    }

    if(filter == "<")
    {
        vs.lessThanSQL(clauses);
    }

    return clauses;
}

void K9Portage::ebuildReader(QString fileName)
{
    QFile input(fileName);
    if(!input.exists())
    {
        qDebug() << "eBuild" << input.fileName() << "does not exist.";
        return;
    }

    if(!input.open(QIODevice::ReadOnly))
    {
        qDebug() << "eBuild" << input.fileName() << "could not be opened for reading.";
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

// Copyright (c) 2021, K9spud LLC.
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

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

K9Portage::K9Portage(QObject *parent) : QObject(parent)
{
    separator.setPattern("([^A-Za-z0-9]+)");
    digitVersion.setPattern("([0-9]+)");
    alphaVersion.setPattern("([A-Za-z]+)");

    stringAssignment.setPattern("(.+)\\s*=\\s*\"([^\"]*)\"");
    variableAssignment.setPattern("(.+)\\s*=\\s*([^\"]*)");
    verCutSingle.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)\\)");
    verCutRange.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)-([0-9]+)\\)");
    var_ref.setPattern("\\$\\{([A-z, 0-9, _]+)\\}");

    dependBasicRE.setPattern("([^~=><].*)/([^:\\n]+)");
    dependVersionRE.setPattern("(~|=|>=|>|<|<=)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*)");
    dependSlotRE.setPattern("([^~=><].*)/([^:\\n]+):([^:\\n]+)");
    dependRepositoryRE.setPattern("(~|=|>=|>|<|<=)(.+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n]+)");

    arch = "arm64";
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

void K9Portage::setVersion(QString v)
{
    version.parse(v);
    vars["PVR"] = version.pvr;
    vars["PV"] = version.pv();
    vars["PR"] = version.pr();
    vars["P"] = QString("%1-%2").arg(vars["PN"]).arg(vars["PV"]);
    vars["PF"] = QString("%1-%2").arg(vars["PN"]).arg(version.pvr);
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
            value.replace(match.capturedStart(), match.capturedLength(), QString("%1.%2").arg(version.cut(i)).arg(version.cut(j)));
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
void K9Portage::applyMasks(QSqlDatabase& db)
{
    VersionString vs;
    QString s;
    QString filter;
    QString category;
    QString package;
    QString version;
    QString repo;
    QString slot;
    QFile input;
    QDir dir;
    QTextStream in;
    QRegularExpressionMatch match;

    QSqlQuery query(db);
    db.transaction();

    QString sql = "update PACKAGE set MASKED=1 where ";
    QString clauses;

    dir.setPath("/etc/portage/package.mask");
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    foreach(QString maskFile, dir.entryList())
    {
        input.setFileName(QString("%1/%2").arg(dir.path()).arg(maskFile));
        if(!input.exists() || !input.open(QIODevice::ReadOnly))
        {
            qDebug() << input.fileName() << "could not be opened for reading.";
            continue;
        }

        in.setDevice(&input);
        while(in.atEnd() == false)
        {
            s = in.readLine();
            s = s.trimmed();
            if(s.isEmpty() || s.startsWith('#'))
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
                filter = match.captured(1);
                category = match.captured(2);
                category.replace("*", "%");
                package = match.captured(3);
                package.replace("*", "%");
                version = match.captured(4);
                version.replace("*", "%");
                slot = match.captured(5);
                slot.replace("*", "%");

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

                qDebug() << "Unknown mask filter type (Depend Repo):" << filter;
                qDebug() << "  " << s;
                continue;
            }

            qDebug() << "Could not parse depend string:";
            qDebug() << "  " << s;
            continue;
        }

        input.close();
    }

    db.commit();
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
        clauses.append(vs.lessThanEqualToSQL());
    }

    if(filter == ">=")
    {
        clauses.append(vs.greaterThanEqualToSQL());
    }

    if(filter == ">")
    {
        clauses.append(vs.greaterThanSQL());
    }

    if(filter == "<")
    {
        clauses.append(vs.lessThanSQL());
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
            value = match.captured(2);
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

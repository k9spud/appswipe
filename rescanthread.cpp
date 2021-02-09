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

#include "rescanthread.h"
#include "datastorage.h"
#include "k9portage.h"
#include "globals.h"
#include "versionstring.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QUuid>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>

RescanThread::RescanThread(QObject *parent) : QThread(parent)
{
}

void RescanThread::rescan()
{
    if(isRunning())
    {
        abort = true;
        wait();
    }

    abort = false;
    start();
}

void RescanThread::run()
{
    QSqlDatabase db;
    if(QSqlDatabase::contains("RescanThread") == false)
    {
        db = QSqlDatabase::addDatabase("QSQLITE", "RescanThread");
    }
    else
    {
        db = QSqlDatabase::database("RescanThread");
        if(db.isValid())
        {
            db.close();
        }
    }

    db.setDatabaseName(ds->storageFolder + ds->databaseFileName);
    db.open();

    QSqlQuery query(db);

    db.transaction();

    if(query.exec("delete from REPO") == false)
    {
        db.rollback();
        return;
    }

    QString repo;
    query.prepare("insert into REPO (REPOID, REPO, LOCATION) values(?, ?, ?)");
    for(int i = 0; i < portage->repos.count(); i++)
    {
        query.bindValue(0, i);
        QString folder = portage->repos.at(i);
        QStringList paths = folder.split('/');
        repo = paths.at(paths.count() - 2);
        query.bindValue(1, repo);
        query.bindValue(2, folder);

        if(query.exec() == false)
        {
            db.rollback();
            return;
        }
    }

    QStringList categories;
    int folderCount = 0, progressCount = 0;
    QDir dir;
    foreach(QString repo, portage->repos)
    {
        dir.setPath(repo);
        dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach(QString cat, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            folderCount++;
            if(categories.contains(cat) == false)
            {
                categories.append(cat);
            }
        }
    }

    dir.setPath("/var/db/pkg/");
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QString cat, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        folderCount++;
        if(categories.contains(cat) == false)
        {
            categories.append(cat);
        }
    }

    if(query.exec("delete from CATEGORY") == false)
    {
        db.rollback();
        return;
    }

    query.prepare("insert into CATEGORY (CATEGORYID, CATEGORY) values(?, ?)");
    for(int i = 0; i < categories.count(); i++)
    {
        query.bindValue(0, i);
        query.bindValue(1, categories.at(i));
        if(query.exec() == false)
        {
            db.rollback();
            return;
        }
    }

    if(query.exec("delete from PACKAGE") == false)
    {
        db.rollback();
        return;
    }

    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString data;
    QString installedFilePath;
    QString packageName;
    int downloadSize = -1;
    qint64 published = 0;
    qint64 installed = 0;
    QDir builds;
    QFile input;
    QFileInfo fi;
    bool ok;

    QString sql = QString(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6, SLOT,
    LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED
)
values
(
    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, 0, ?, ?, ?, 0, ?
)
)EOF");
    query.prepare(sql);
    for(int repoId = 0; repoId < portage->repos.count(); repoId++)
    {
        qDebug() << "Scanning repo:" << portage->repos.at(repoId);
        for(int categoryId = 0; categoryId < categories.count(); categoryId++)
        {
            if(abort)
            {
                break;
            }

            categoryPath = portage->repos.at(repoId);
            categoryPath.append(categories.at(categoryId));
            dir.setPath(categoryPath);
            if(dir.exists() == false)
            {
                continue;
            }

            foreach(packageName, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
            {
                buildsPath = categoryPath;
                buildsPath.append('/');
                buildsPath.append(packageName);
                builds.setPath(buildsPath);
                builds.setNameFilters(QStringList("*.ebuild"));
                foreach(ebuildFilePath, builds.entryList(QDir::Files))
                {
                    portage->vars.clear();
                    portage->vars["PN"] = packageName;
                    portage->setVersion(ebuildFilePath.mid(packageName.length() + 1, ebuildFilePath.length() - (7 + packageName.length() + 1)));

                    downloadSize = -1;
                    installed = 0;
                    installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(categories.at(categoryId)).arg(packageName).arg(portage->version.pvr);
                    fi.setFile(installedFilePath);
                    if(fi.exists())
                    {
                        installed = fi.birthTime().toSecsSinceEpoch();
                        input.setFileName(QString("%1/SIZE").arg(installedFilePath));
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

                    fi.setFile(buildsPath + "/" + ebuildFilePath);
                    published = fi.birthTime().toSecsSinceEpoch();

                    portage->ebuildReader(buildsPath + "/" + ebuildFilePath);

                    query.bindValue(0, categoryId);
                    query.bindValue(1, repoId);
                    query.bindValue(2, packageName);
                    query.bindValue(3, portage->var("DESCRIPTION"));
                    query.bindValue(4, portage->var("HOMEPAGE"));
                    query.bindValue(5, portage->version.pvr);
                    query.bindValue(6, portage->version.cut(0));
                    query.bindValue(7, portage->version.cut(1));
                    query.bindValue(8, portage->version.cut(2));
                    query.bindValue(9, portage->version.cut(3));
                    query.bindValue(10, portage->version.cut(4));
                    query.bindValue(11, portage->var("PR"));
                    query.bindValue(12, portage->var("SLOT"));
                    query.bindValue(13, portage->var("LICENSE"));
                    query.bindValue(14, installed);
                    query.bindValue(15, downloadSize);
                    query.bindValue(16, portage->var("KEYWORDS"));
                    query.bindValue(17, portage->var("IUSE"));
                    query.bindValue(18, published);
                    if(query.exec() == false)
                    {
                        qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                        db.rollback();
                        return;
                    }
                }
            }

            emit progress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount));
        }
    }

    QString repoFilePath;
    bool obsolete;
    int repoId = 0;
    query.prepare(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6,
    SLOT, LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED
)
values
(
    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?, 0, ?
)
)EOF");

    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");
    QString package;

    qDebug() << "Scanning installed packages...";
    for(int categoryId = 0; categoryId < categories.count(); categoryId++)
    {
        if(abort)
        {
            break;
        }

        categoryPath = QString("/var/db/pkg/%1/").arg(categories.at(categoryId));
        dir.setPath(categoryPath);
        if(dir.exists() == false)
        {
            continue;
        }

        foreach(package, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
        {
            portage->vars.clear();
            portage->vars["PN"] = packageName = package;

            match = pvsplit.match(package, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
            if(match.hasMatch() && match.lastCapturedIndex() >= 2)
            {
                portage->vars["PN"] = packageName = match.captured(1);
                portage->setVersion(match.captured(2));
            }
            else
            {
                portage->setVersion("0-0-0-0");
            }

            buildsPath = categoryPath;
            buildsPath.append(package);
            repoFilePath = QString("%1/repository").arg(buildsPath);
            input.setFileName(repoFilePath);
            if(!input.open(QIODevice::ReadOnly))
            {
                qDebug() << "Can't open repository file:" << input.fileName();
            }
            else
            {
                data = input.readAll();
                input.close();
                data = data.trimmed();

                installedFilePath = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(data).arg(categories.at(categoryId)).arg(packageName).arg(portage->version.pvr);
                if(QFile::exists(installedFilePath))
                {
                    // already imported this package from the repo directory
                    continue;
                }

                repo = QString("/var/db/repos/%1/").arg(data);
                repoId = -1;
                for(int i = 0; i < portage->repos.count(); i++)
                {
                    if(repo == portage->repos.at(i))
                    {
                        repoId = i;
                        break;
                    }
                }

                downloadSize = -1;
                input.setFileName(QString("%1/SIZE").arg(buildsPath));
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

                ebuildFilePath = QString("%1/%2.ebuild").arg(buildsPath).arg(package);
                fi.setFile(ebuildFilePath);
                installed = fi.birthTime().toSecsSinceEpoch();
                published = 0;
                obsolete = true;

                portage->ebuildReader(ebuildFilePath);

                query.bindValue(0, categoryId);
                if(repoId == -1)
                {
                    query.bindValue(1, QVariant(QVariant::Int)); // NULL repoId
                }
                else
                {
                    query.bindValue(1, repoId);
                }
                query.bindValue(2, packageName);
                query.bindValue(3, portage->var("DESCRIPTION"));
                query.bindValue(4, portage->var("HOMEPAGE"));
                query.bindValue(5, portage->version.pvr);
                query.bindValue(6, portage->version.cut(0));
                query.bindValue(7, portage->version.cut(1));
                query.bindValue(8, portage->version.cut(2));
                query.bindValue(9, portage->version.cut(3));
                query.bindValue(10, portage->version.cut(4));
                query.bindValue(11, portage->version.pr());
                query.bindValue(12, portage->var("SLOT"));
                query.bindValue(13, portage->var("LICENSE"));
                query.bindValue(14, installed);
                query.bindValue(15, obsolete);
                query.bindValue(16, downloadSize);
                query.bindValue(17, portage->var("KEYWORDS"));
                query.bindValue(18, portage->var("IUSE"));
                query.bindValue(19, published);
                if(query.exec() == false)
                {
                    qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                    db.rollback();
                    return;
                }
            }
        }

        emit progress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount));
    }
    db.commit();
/*
    db.transaction();
    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%9999%'") == false)
    {
        db.rollback();
        return;
    }

    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%_alpha%'") == false)
    {
        db.rollback();
        return;
    }

    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%_beta%'") == false)
    {
        db.rollback();
        return;
    }
    db.commit();
*/
    emit progress(100);
    qDebug() << "Rescan thread finished.";
}

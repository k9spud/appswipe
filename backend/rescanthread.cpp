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

void RescanThread::reloadDatabase()
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

    QString category;
    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString packageName;
    QString metaCacheFilePath;
    QFileInfo fi;
    QDir builds;

    query.prepare(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, SLOT, LICENSE, INSTALLED, OBSOLETED,
    DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED, STATUS, SUBSLOT,
    V1, V2, V3, V4, V5, V6, V7, V8, V9, V10
)
values
(
    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
)
)EOF");
    for(int repoId = 0; repoId < portage->repos.count(); repoId++)
    {
        output << QString("Loading %1").arg(portage->repos.at(repoId)) << Qt::endl;
        for(int categoryId = 0; categoryId < categories.count(); categoryId++)
        {
            if(abort)
            {
                break;
            }

            category = categories.at(categoryId);
            categoryPath = portage->repos.at(repoId);
            categoryPath.append(category);
            dir.setPath(categoryPath);
            if(dir.exists() == false)
            {
                continue;
            }

            foreach(packageName, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
            {
                buildsPath = QString("%1/%2").arg(categoryPath, packageName);
                builds.setPath(buildsPath);
                builds.setNameFilters(QStringList("*.ebuild"));
                foreach(ebuildFilePath, builds.entryList(QDir::Files))
                {
                    query.bindValue(0, categoryId);
                    query.bindValue(1, repoId);

                    metaCacheFilePath = QString("%1metadata/md5-cache/%2/%3").arg(portage->repos.at(repoId), category, ebuildFilePath.left(ebuildFilePath.length() - 7));
                    fi.setFile(metaCacheFilePath);
                    if(fi.exists())
                    {
                        fi.setFile(buildsPath + "/" + ebuildFilePath);
                        query.bindValue(14, fi.birthTime().toSecsSinceEpoch()); /* published */

                        if(importMetaCache(&query, category, packageName, metaCacheFilePath, ebuildFilePath.mid(packageName.length() + 1, ebuildFilePath.length() - (7 + packageName.length() + 1))) == false)
                        {
                            output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                            db.rollback();
                            return;
                        }
                    }
                    else if(importRepoPackage(&query, category, packageName, buildsPath, ebuildFilePath) == false)
                    {
                        output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                        db.rollback();
                        return;
                    }
                }
            }

            progress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount));
        }
    }

    QString package;

    output << "Loading /var/db/pkg/" << Qt::endl;
    for(int categoryId = 0; categoryId < categories.count(); categoryId++)
    {
        if(abort)
        {
            break;
        }

        category = categories.at(categoryId);
        categoryPath = QString("/var/db/pkg/%1/").arg(category);
        dir.setPath(categoryPath);
        if(dir.exists() == false)
        {
            continue;
        }

        foreach(package, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
        {
            query.bindValue(0, categoryId);
            if(importInstalledPackage(&query, category, package) == false)
            {
                output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                db.rollback();
                return;
            }
        }

        progress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount));
    }
    db.commit();
    output << "Applying masks..." << Qt::endl;
    int progressCountMasks = 100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount);
    portage->applyMasks(db, progressCountMasks);
    progress(100);
    output << "Done." << Qt::endl;
}

void RescanThread::reloadApp(QString atom)
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

    query.prepare("delete from PACKAGE where CATEGORYID=(select CATEGORYID from CATEGORY where CATEGORY=?) and PACKAGE=?");
    QStringList x = atom.split('/');
    query.bindValue(0, x.first());
    query.bindValue(1, x.last());

    if(query.exec() == false)
    {
        db.rollback();
        return;
    }
    progress(1);

    QString category = x.first();
    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString metaCacheFilePath;
    QFileInfo fi;
    QDir builds;
    QString packageName = x.last();
    int progressCount = 2;

    query.prepare(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, SLOT, LICENSE, INSTALLED, OBSOLETED,
    DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED, STATUS, SUBSLOT,
    V1, V2, V3, V4, V5, V6, V7, V8, V9, V10
)
values
(
    (select CATEGORYID from CATEGORY where CATEGORY=?), ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
)
)EOF");
    for(int repoId = 0; repoId < portage->repos.count(); repoId++)
    {
        categoryPath = portage->repos.at(repoId);
        categoryPath.append(category);

        buildsPath = QString("%1/%2").arg(categoryPath, packageName);
        builds.setPath(buildsPath);
        builds.setNameFilters(QStringList("*.ebuild"));
        foreach(ebuildFilePath, builds.entryList(QDir::Files))
        {
            query.bindValue(0, category);
            query.bindValue(1, repoId);

            metaCacheFilePath = QString("%1metadata/md5-cache/%2/%3").arg(portage->repos.at(repoId), category, ebuildFilePath.left(ebuildFilePath.length() - 7));
            fi.setFile(metaCacheFilePath);
            if(fi.exists())
            {
                fi.setFile(buildsPath + "/" + ebuildFilePath);
                query.bindValue(14, fi.birthTime().toSecsSinceEpoch()); /* published */

                if(importMetaCache(&query, category, packageName, metaCacheFilePath, ebuildFilePath.mid(packageName.length() + 1, ebuildFilePath.length() - (7 + packageName.length() + 1))) == false)
                {
                    output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                    db.rollback();
                    return;
                }
            }
            else if(importRepoPackage(&query, category, packageName, buildsPath, ebuildFilePath) == false)
            {
                output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                db.rollback();
                return;
            }
            progress(progressCount++);
        }
    }

    QString package;

    QStringList packageNameFilter;
    packageNameFilter << QString("%1-*").arg(packageName);

    QDir dir;
    categoryPath = QString("/var/db/pkg/%1/").arg(category);
    dir.setPath(categoryPath);
    foreach(package, dir.entryList(packageNameFilter, QDir::Dirs | QDir::NoDotAndDotDot))
    {
        query.bindValue(0, category);
        if(importInstalledPackage(&query, category, package) == false)
        {
            output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
            db.rollback();
            return;
        }
        progress(progressCount++);
    }

    db.commit();
    progress(progressCount++);

    portage->applyMasks(db, progressCount, atom);

    progress(100);
}

bool RescanThread::importInstalledPackage(QSqlQuery* query, QString category, QString package)
{
    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");

    QString categoryPath = QString("/var/db/pkg/%1/").arg(category);
    QString buildsPath;
    QString ebuildFilePath;
    QString data;
    QString installedFilePath;
    QString metaCacheFilePath;
    QString repoFilePath;
    QString packageName;
    QString slot;
    QString subslot;
    int downloadSize = -1;
    qint64 published = 0;
    qint64 status = K9Portage::UNKNOWN;
    QString keywords;
    QString iuse;
    QString license;
    QString description;
    QString homepage;
    QStringList keywordList;
    qint64 installed = 0;
    QFile input;
    QFileInfo fi;
    bool ok;
    QString repo;
    bool obsolete = true;
    int repoId = 0;

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
        output << "Can't open repository file:" << input.fileName() << Qt::endl;
        return true;
    }

    data = input.readAll();
    input.close();
    data = data.trimmed();

    metaCacheFilePath = QString("/var/db/repos/%1/metadata/md5-cache/%2/%3-%4").arg(data, category, packageName, portage->version.pvr);
    if(QFile::exists(metaCacheFilePath))
    {
        obsolete = false;
        ebuildFilePath = metaCacheFilePath;
    }
    else
    {
        installedFilePath = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(data, category, packageName, portage->version.pvr);
        if(QFile::exists(installedFilePath))
        {
            obsolete = false;
            ebuildFilePath = installedFilePath;
        }
        else
        {
            ebuildFilePath = QString("%1/%2.ebuild").arg(buildsPath, package);
        }
    }

    fi.setFile(ebuildFilePath);
    installed = fi.birthTime().toSecsSinceEpoch();
    published = 0;

    portage->ebuildReader(ebuildFilePath);

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

    description = portage->var("DESCRIPTION").toString();
    input.setFileName(QString("%1/DESCRIPTION").arg(buildsPath));
    if(input.open(QIODevice::ReadOnly))
    {
        data = input.readAll();
        input.close();
        description = data.trimmed();
    }

    homepage = portage->var("HOMEPAGE").toString();
    input.setFileName(QString("%1/HOMEPAGE").arg(buildsPath));
    if(input.open(QIODevice::ReadOnly))
    {
        data = input.readAll();
        input.close();
        homepage = data.trimmed();
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

    keywords = portage->var("KEYWORDS").toString();
    if(keywords.isEmpty()) // don't override the metaCache data for KEYWORDS -- repo may have marked stable something we previously installed during testing.
    {
        input.setFileName(QString("%1/KEYWORDS").arg(buildsPath));
        if(input.open(QIODevice::ReadOnly))
        {
            data = input.readAll();
            input.close();
            keywords = data.trimmed();
        }
    }
    keywordList = keywords.split(' ');
    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
    }
    else
    {
        status = K9Portage::UNKNOWN;
    }

    iuse = portage->var("IUSE").toString();
    if(iuse.isEmpty())
    {
        input.setFileName(QString("%1/IUSE").arg(buildsPath));
        if(input.open(QIODevice::ReadOnly))
        {
            data = input.readAll();
            input.close();
            iuse = data.trimmed();
        }
    }

    if(repoId == -1)
    {
        query->bindValue(1, QVariant(QVariant::Int)); // NULL repoId
    }
    else
    {
        query->bindValue(1, repoId);
    }

    slot = portage->var("SLOT").toString();
    input.setFileName(QString("%1/SLOT").arg(buildsPath));
    if(input.open(QIODevice::ReadOnly))
    {
        data = input.readAll();
        input.close();
        slot = data.trimmed();
    }
    if(slot.contains('/'))
    {
        int ix = slot.indexOf('/');
        subslot = slot.mid(ix + 1);
        slot = slot.left(ix);
    }
    else
    {
        subslot.clear();
    }

    license = portage->var("LICENSE").toString();
    input.setFileName(QString("%1/LICENSE").arg(buildsPath));
    if(input.open(QIODevice::ReadOnly))
    {
        data = input.readAll();
        input.close();
        license = data.trimmed();
    }

    query->bindValue(2, packageName);
    query->bindValue(3, description);
    query->bindValue(4, homepage);
    query->bindValue(5, portage->version.pvr);
    query->bindValue(6, QVariant(slot));
    query->bindValue(7, license);
    query->bindValue(8, installed);
    query->bindValue(9, obsolete);
    query->bindValue(10, downloadSize);
    query->bindValue(11, keywords);
    query->bindValue(12, iuse);
    query->bindValue(13, false); /* not masked if installed */
    query->bindValue(14, published);
    query->bindValue(15, status);
    query->bindValue(16, QVariant(subslot));
    query->bindValue(17, portage->version.cutInternalVx(0));
    query->bindValue(18, portage->version.cutInternalVx(1));
    query->bindValue(19, portage->version.cutInternalVx(2));
    query->bindValue(20, portage->version.cutInternalVx(3));
    query->bindValue(21, portage->version.cutInternalVx(4));
    query->bindValue(22, portage->version.cutInternalVx(5));
    query->bindValue(23, portage->version.cutInternalVx(6));
    query->bindValue(24, portage->version.cutInternalVx(7));
    query->bindValue(25, portage->version.cutInternalVx(8));
    query->bindValue(26, portage->version.revision());
    if(query->exec() == false)
    {
        return false;
    }

    return true;
}

bool RescanThread::importRepoPackage(QSqlQuery* query, QString category, QString packageName, QString buildsPath, QString ebuildFilePath)
{
    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");

    QString installedFilePath;
    QString slot;
    QString subslot;
    int downloadSize;
    qint64 published = 0;
    qint64 status = K9Portage::UNKNOWN;
    QString keywords;
    QStringList keywordList;
    qint64 installed = 0;
    QFileInfo fi;
    QFile input;
    QString data;
    bool ok;

    portage->vars.clear();
    portage->vars["PN"] = packageName;
    portage->setVersion(ebuildFilePath.mid(packageName.length() + 1, ebuildFilePath.length() - (7 + packageName.length() + 1)));

    installed = 0;
    installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(category, packageName, portage->version.pvr);
    fi.setFile(installedFilePath);
    if(fi.exists())
    {
        return true;
    }

    fi.setFile(buildsPath + "/" + ebuildFilePath);
    published = fi.birthTime().toSecsSinceEpoch();

    portage->ebuildReader(buildsPath + "/" + ebuildFilePath);

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

    keywords = portage->var("KEYWORDS").toString();
    keywordList = keywords.split(' ');
    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
    }
    else
    {
        status = K9Portage::UNKNOWN;
    }

    slot = portage->var("SLOT").toString();
    if(slot.contains('/'))
    {
        int ix = slot.indexOf('/');
        subslot = slot.mid(ix + 1);
        slot = slot.left(ix);
    }
    else
    {
        subslot.clear();
    }
    query->bindValue(2, packageName);
    query->bindValue(3, portage->var("DESCRIPTION"));
    query->bindValue(4, portage->var("HOMEPAGE"));
    query->bindValue(5, portage->version.pvr);
    query->bindValue(6, QVariant(slot));
    query->bindValue(7, portage->var("LICENSE"));
    query->bindValue(8, installed);
    query->bindValue(9, 0); /* not obsolete if repo still has .ebuild file */
    query->bindValue(10, downloadSize);
    query->bindValue(11, keywords);
    query->bindValue(12, portage->var("IUSE"));
    query->bindValue(13, 0); /* default to not masked for now -- masks applied later as last step */
    query->bindValue(14, published);
    query->bindValue(15, status);
    query->bindValue(16, QVariant(subslot));
    query->bindValue(17, portage->version.cutInternalVx(0));
    query->bindValue(18, portage->version.cutInternalVx(1));
    query->bindValue(19, portage->version.cutInternalVx(2));
    query->bindValue(20, portage->version.cutInternalVx(3));
    query->bindValue(21, portage->version.cutInternalVx(4));
    query->bindValue(22, portage->version.cutInternalVx(5));
    query->bindValue(23, portage->version.cutInternalVx(6));
    query->bindValue(24, portage->version.cutInternalVx(7));
    query->bindValue(25, portage->version.cutInternalVx(8));
    query->bindValue(26, portage->version.revision());
    if(query->exec() == false)
    {
        return false;
    }

    return true;
}

bool RescanThread::importMetaCache(QSqlQuery* query, QString category, QString packageName, QString metaCacheFilePath, QString version)
{
    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");

    QString installedFilePath;
    QString slot;
    QString subslot;
    int downloadSize;
    qint64 status = K9Portage::UNKNOWN;
    QString keywords;
    QStringList keywordList;
    qint64 installed = 0;
    QFileInfo fi;
    QFile input;
    QString data;
    bool ok;

    portage->vars.clear();
    portage->vars["PN"] = packageName;
    portage->setVersion(version);

    installed = 0;
    installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(category, packageName, portage->version.pvr);
    fi.setFile(installedFilePath);
    if(fi.exists())
    {
        return true;
    }

    portage->ebuildReader(metaCacheFilePath);

    downloadSize = -1;
    input.setFileName(QString("/var/db/pkg/%1/%2-%3/SIZE").arg(category, packageName,  portage->version.pvr));
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

    keywords = portage->var("KEYWORDS").toString();
    keywordList = keywords.split(' ');
    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
    }
    else
    {
        status = K9Portage::UNKNOWN;
    }

    slot = portage->var("SLOT").toString();
    if(slot.contains('/'))
    {
        int ix = slot.indexOf('/');
        subslot = slot.mid(ix + 1);
        slot = slot.left(ix);
    }
    else
    {
        subslot.clear();
    }
    query->bindValue(2, packageName);
    query->bindValue(3, portage->var("DESCRIPTION"));
    query->bindValue(4, portage->var("HOMEPAGE"));
    query->bindValue(5, portage->version.pvr);
    query->bindValue(6, QVariant(slot));
    query->bindValue(7, portage->var("LICENSE"));
    query->bindValue(8, installed);
    query->bindValue(9, 0); /* not obsolete if repo still has .ebuild file */
    query->bindValue(10, downloadSize);
    query->bindValue(11, keywords);
    query->bindValue(12, portage->var("IUSE"));
    query->bindValue(13, 0); /* default to not masked for now -- masks applied later as last step */
    query->bindValue(15, status);
    query->bindValue(16, QVariant(subslot));
    query->bindValue(17, portage->version.cutInternalVx(0));
    query->bindValue(18, portage->version.cutInternalVx(1));
    query->bindValue(19, portage->version.cutInternalVx(2));
    query->bindValue(20, portage->version.cutInternalVx(3));
    query->bindValue(21, portage->version.cutInternalVx(4));
    query->bindValue(22, portage->version.cutInternalVx(5));
    query->bindValue(23, portage->version.cutInternalVx(6));
    query->bindValue(24, portage->version.cutInternalVx(7));
    query->bindValue(25, portage->version.cutInternalVx(8));
    query->bindValue(26, portage->version.revision());
    if(query->exec() == false)
    {
        return false;
    }

    return true;
}

void RescanThread::run()
{
    reloadDatabase();
    output << "Rescan thread finished." << Qt::endl;
}

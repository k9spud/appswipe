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

#include "importvdb.h"
#include "datastorage.h"
#include "k9portage.h"
#include "globals.h"
#include "importvdb.h"
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

ImportVDB::ImportVDB()
{
}

int ImportVDB::loadCategories(QStringList& categories, const QString folder)
{
    QDir dir;
    int folderCount = 0;
    int i;

    dir.setPath(folder);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QString s;
    const int foldersCount = folders.count();
    for(i = 0; i < foldersCount; i++)
    {
        folderCount++;
        s = folders.at(i);
        if(categories.contains(s) == false)
        {
            categories.append(s);
        }
    }

    return folderCount;
}

void ImportVDB::reloadDatabase()
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
    QString s;
    QStringList paths;
    int i;
    query.prepare("insert into REPO (REPOID, REPO, LOCATION) values(?, ?, ?)");
    const int repoCount = portage->repos.count();
    for(i = 0; i < repoCount; i++)
    {
        query.bindValue(0, i);
        s = portage->repos.at(i);
        paths = s.split('/');
        repo = paths.at(paths.count() - 2);
        query.bindValue(1, repo);
        query.bindValue(2, s);

        if(query.exec() == false)
        {
            db.rollback();
            return;
        }
    }

    QStringList categories;
    int folderCount = 0, progressCount = 0;
    QDir dir;
    for(i = 0; i < repoCount; i++)
    {
        folderCount += loadCategories(categories, portage->repos.at(i));
    }
    folderCount += loadCategories(categories, "/var/db/pkg/");

    if(query.exec("delete from CATEGORY") == false)
    {
        db.rollback();
        return;
    }

    query.prepare("insert into CATEGORY (CATEGORYID, CATEGORY) values(?, ?)");
    const int categoryCount = categories.count();
    for(i = 0; i < categoryCount; i++)
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
    QStringList packageFolders;
    QStringList ebuildFiles;
    int folder;
    int categoryId;

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
    for(int repoId = 0; repoId < repoCount; repoId++)
    {
        output << QString("Loading %1").arg(portage->repos.at(repoId)) << Qt::endl;
        for(categoryId = 0; categoryId < categoryCount; categoryId++)
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
            packageFolders = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
            const int packageFolderCount = packageFolders.count();
            for(folder = 0; folder < packageFolderCount; folder++)
            {
                packageName = packageFolders.at(folder);
                buildsPath = QString("%1/%2").arg(categoryPath, packageName);
                builds.setPath(buildsPath);
                builds.setNameFilters(QStringList("*.ebuild"));
                ebuildFiles = builds.entryList(QDir::Files);
                const int ebuildCount = ebuildFiles.count();
                for(i = 0; i < ebuildCount; i++)
                {
                    ebuildFilePath = ebuildFiles.at(i);
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

    output << "Loading /var/db/pkg/" << Qt::endl;
    for(categoryId = 0; categoryId < categoryCount; categoryId++)
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

        packageFolders = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        const int packageFolderCount = packageFolders.count();
        for(i = 0; i < packageFolderCount; i++)
        {
            packageName = packageFolders.at(i);
            query.bindValue(0, categoryId);
            if(importInstalledPackage(&query, category, packageName) == false)
            {
                output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                db.rollback();
                return;
            }
        }

        progress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(folderCount));
    }
    db.commit();
    progress(100);
    output << "Done." << Qt::endl;
}

void ImportVDB::reloadApp(QStringList appsList)
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
    int i, repoId, file;
    QStringList sl;
    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString metaCacheFilePath;
    QString category;
    QString packageName;
    QFileInfo fi;
    QDir builds;
    QStringList ebuildFiles;

    const int appsCount = appsList.count();
    for(i = 0; i < appsCount; i++)
    {
        sl = appsList.at(i).split('/');
        category = sl.first();
        packageName = sl.last();

        query.bindValue(0, category);
        query.bindValue(1, packageName);
        if(query.exec() == false)
        {
            db.rollback();
            return;
        }
    }

    progress(1);
    int progressCount = 2;

    query.prepare(QStringLiteral(R"EOF(
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
)EOF"));

    const int repoCount = portage->repos.count();
    for(i = 0; i < appsCount; i++)
    {
        sl = appsList.at(i).split('/');
        category = sl.first();
        packageName = sl.last();
        query.bindValue(0, category);

        for(repoId = 0; repoId < repoCount; repoId++)
        {
            query.bindValue(1, repoId);

            categoryPath = portage->repos.at(repoId);
            categoryPath.append(category);

            buildsPath = QString("%1/%2").arg(categoryPath, packageName);
            builds.setPath(buildsPath);
            builds.setNameFilters(QStringList("*.ebuild"));
            ebuildFiles = builds.entryList(QDir::Files);
            const int ebuildCount = ebuildFiles.count();
            for(file = 0; file < ebuildCount; file++)
            {
                ebuildFilePath = ebuildFiles.at(file);

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
        query.bindValue(0, category);

        ebuildFiles = dir.entryList(packageNameFilter, QDir::Dirs | QDir::NoDotAndDotDot);
        const int ebuildCount = ebuildFiles.count();
        for(file = 0; file < ebuildCount; file++)
        {
            package = ebuildFiles.at(file);
            if(importInstalledPackage(&query, category, package) == false)
            {
                output << "Query failed:" << query.executedQuery() << query.lastError().text() << Qt::endl;
                db.rollback();
                return;
            }
            progress(progressCount++);
        }
    }

    db.commit();
    progress(100);
}

int ImportVDB::readConfigFolder(/*QSqlQuery& query,*/ QString fileFolder)
{
    readMakeConf(QString("%1/make.defaults").arg(fileFolder));

    int result;
    result = readConfigFile(fileFolder, QStringLiteral("package.accept_keywords"), K9AtomAction::packageAcceptKeywords);
    if(result)
    {
        return result;
    }

/*
    result = readConfigFile(fileFolder, QStringLiteral("package.keywords"), K9AtomAction::packageKeywords);
    if(result)
    {
        return result;
    }
*/

    result = readConfigFile(fileFolder, QStringLiteral("package.mask"), K9AtomAction::packageMask);
    if(result)
    {
        return result;
    }
/*
    result = readConfigFile(fileFolder, QStringLiteral("package.provided"), K9AtomAction::packageProvided);
    if(result)
    {
        return result;
    }
*/
    result = readConfigFile(fileFolder, QStringLiteral("package.unmask"), K9AtomAction::packageUnmask);
    if(result)
    {
        return result;
    }
/*
    result = readConfigFile(fileFolder, QStringLiteral("package.use"), K9AtomAction::packageUse);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("package.use.force"), K9AtomAction::packageUseForce);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("package.use.mask"), K9AtomAction::packageUseMask);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("package.use.stable.force"), K9AtomAction::packageUseStableForce);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("package.use.stable.mask"), K9AtomAction::packageUseStableMask);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("use.force"), K9AtomAction::useForce);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("use.mask"), K9AtomAction::useMask);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("use.stable.mask"), K9AtomAction::useStableMask);
    if(result)
    {
        return result;
    }

    result = readConfigFile(fileFolder, QStringLiteral("use.stable.force"), K9AtomAction::useStableForce);
    if(result)
    {
        return result;
    }
*/
    return 0;
}

int ImportVDB::readProfileFolder(QString profileFolder)
{
    int result;

    if(profileFolders.contains(profileFolder))
    {
        // don't get stuck in an infinite loop of repeating profile folders
        return 0;
    }
    profileFolders.append(profileFolder);

    // read parent profiles first
    QString s = QString("%1/parent").arg(profileFolder);
    QFileInfo fi;
    fi.setFile(s);
    if(fi.isFile())
    {
        QFile input;
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString wholeFile = input.readAll();
            input.close();
            QStringList lines = wholeFile.split('\n');
            QString parent;
            const int linesCount = lines.count();
            for(int i = 0; i < linesCount; i++)
            {
                parent = QString("%1/%2").arg(profileFolder, lines.at(i));
                fi.setFile(parent);
                if(fi.isDir())
                {
                    result = readProfileFolder(fi.canonicalFilePath());
                    if(result)
                    {
                        return result;
                    }
                }
            }
        }
    }

    result = readConfigFolder(profileFolder);
    if(result)
    {
        return result;
    }

    return 0;
}

void ImportVDB::readMakeConf(QString filePath)
{
    QFile input;
    input.setFileName(filePath);
    QString varkey;
    if(input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QStringList lines = static_cast<QString>(input.readAll()).split('\n');
        input.close();

        const int linesCount = lines.count();
        QString s;
        QString key;
        QString value;
        int i;
        for(int j = 0; j < linesCount; j++)
        {
            s = lines.at(j).trimmed();
            if(s.startsWith('#') || s.isEmpty())
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
            if(value.startsWith('"'))
            {
                while(value.endsWith('"') == false && ++i < linesCount)
                {
                    s = lines.at(i);
                    value.append(s);
                }

                if(value.endsWith('"'))
                {
                    value = value.mid(1, value.count() - 2);
                }
                else
                {
                    value = value.mid(1);
                }
            }

            varkey = QString("${%1}").arg(key);
            if(value.contains(varkey))
            {
                value.remove(varkey);
            }

            if(makeConf.contains(key))
            {
                value.prepend(' ');
            }

            makeConf[key].append(value);
        }
    }
}

void ImportVDB::loadConfig()
{
    QString s;
    QFileInfo fi;
    const int reposCount = portage->repos.count();
    for(int i = 0; i < reposCount; i++)
    {
        s = QString("%1profiles").arg(portage->repos.at(i));
        if(readConfigFolder(s))
        {
            return;
        }
    }

    fi.setFile("/etc/portage/make.profile");
    if(fi.isDir())
    {
        readProfileFolder(fi.canonicalFilePath());
    }

    s = QStringLiteral("/etc/portage");
    if(readConfigFolder(s))
    {
        return;
    }

    readMakeConf("/etc/portage/make.conf");

    profileFolders.clear();
}

int ImportVDB::readConfigFile(QString fileFolder, QString fileName, K9AtomAction::AtomActionType actionType)
{
    int result;
    QString fileAndPath = QString("%1/%2").arg(fileFolder, fileName);
    QFileInfo fi;
    fi.setFile(fileAndPath);
    if(fi.isDir())
    {
        QDir dir;
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setPath(fi.absoluteFilePath());
        foreach(QString file, dir.entryList())
        {
            result = readConfigFile(dir.path(), file, actionType);
            if(result)
            {
                return result;
            }
        }
        return 0;
    }

    if(fi.exists() == false || fi.isFile() == false)
    {
        return 0;
    }

    QFile input;
    input.setFileName(fileAndPath);
    if(!input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return 0;
    }

    const static QList<K9AtomAction::AtomActionType> anyPackageActionType = { K9AtomAction::useForce, K9AtomAction::useMask, K9AtomAction::useStableMask, K9AtomAction::useStableForce };
    bool anyPackage = false;
    if(anyPackageActionType.contains(actionType))
    {
        anyPackage = true;
    }

    QStringList lines = static_cast<QString>(input.readAll()).split('\n');
    input.close();

    QString atom;
    QString action;
    QString comment;
    QString s;
    int spaceIndex;
    int atomId;
    bool newComment = true;
    bool negative;
    const int linesCount = lines.count();
    for(int i = 0; i < linesCount; i++)
    {
        s = lines.at(i).trimmed();
        if(s.isEmpty())
        {
            newComment = true;
            continue;
        }

        if(s.startsWith('#'))
        {
            if(newComment)
            {
                comment.clear();
            }
            else
            {
                comment.append("\n");
            }
            comment.append(s.mid(1).trimmed());
            newComment = false;
            continue;
        }

        if(s.startsWith('-'))
        {
            s = s.mid(1);
            negative = true;
        }
        else
        {
            negative = false;
        }

        if(anyPackage)
        {
            atom = "*/*";
            action = s;
        }
        else
        {
            spaceIndex = s.indexOf(' ');
            if(spaceIndex > 0)
            {
                atom = s.left(spaceIndex);
                action = s.mid(spaceIndex + 1).trimmed();
            }
            else
            {
                atom = s;
                action.clear();
            }
        }

        atomId = atoms.indexOf(atom);
        if(atomId < 0)
        {
            if(negative)
            {
                // normally, a negative line should remove all prior matching lines.
                // but if no prior matching atom, obviously there aren't any prior matching lines to remove
                newComment = true;
                continue;
            }
            atomId = atoms.count();
            atoms.append(atom);
            atomList.appendAtom(atomId, atom);
        }

        K9AtomAction a;
        a.actionType = actionType;
        a.action = action;
        if(negative)
        {
            atomList.atomActions[atomId].removeAll(a);
        }
        else
        {
            atomList.appendAtomAction(atomId, a);
        }

        newComment = true;
    }

    return 0;
}

void ImportVDB::applyConfigMasks(K9Atom::maskType& masked, QString category, QString package, QString slot, QString subslot, QStringList keywordList)
{
    QList<int> matchingAtomIds = atomList.findMatches(category, package, slot, subslot, portage->version);
    const int matchingIdsCount = matchingAtomIds.count();
    int atomId;
    QStringList sl;
    int i;

    for(i = 0; i < matchingIdsCount; i++)
    {
        atomId = matchingAtomIds.at(i);

        QList<K9AtomAction> actionList = atomList.atomActions.value(atomId);
        const int actionListCount = actionList.count();
        for(int k = 0; k < actionListCount; k++)
        {
            K9AtomAction action = actionList.at(k);
            switch(action.actionType)
            {
                case K9AtomAction::packageAcceptKeywords:
                    if(action.action.isEmpty())
                    {
                        // Lines without any accept_keywords imply unstable host arch.
                        masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::testingMask));
                    }
                    else
                    {
                        QStringList allowed = action.action.split(' ', Qt::SkipEmptyParts);
                        const int allowedCount = allowed.count();
                        for(int i = 0; i < allowedCount; i++)
                        {
                            const QString& s = allowed.at(i);
                            if(s == "**")
                            {
                                // Package is always visible (KEYWORDS are ignored completely)
                                masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::testingMask) & ~(K9Atom::unsupportedMask) & ~(K9Atom::brokenMask));
                            }
                            else if(s == "*")
                            {
                                // Package is visible if it is stable on any architecture.
                                QString s;
                                foreach(s, keywordList)
                                {
                                    if(s.isEmpty() == false && s.startsWith("~") == false && s.startsWith("-") == false)
                                    {
                                        masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::testingMask) & ~(K9Atom::unsupportedMask) & ~(K9Atom::brokenMask));
                                        break;
                                    }
                                }
                            }
                            else if(s == "~*")
                            {
                                // Package is visible if it is in testing on any architecture.
                                QString s;
                                foreach(s, keywordList)
                                {
                                    if(s.isEmpty() == false && s.startsWith("-") == false && s.startsWith("~") == true)
                                    {
                                        masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::testingMask) & ~(K9Atom::unsupportedMask) & ~(K9Atom::brokenMask));
                                    }
                                }
                            }
                            else
                            {
                                // Package is visible if keywords match package.accept_keywords
                                if(keywordList.contains(s))
                                {
                                    masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::testingMask) & ~(K9Atom::unsupportedMask) & ~(K9Atom::brokenMask));
                                }
                            }
                        }
                    }
                    break;

/*                case K9AtomAction::packageKeywords:
                    break;
*/
                case K9AtomAction::packageMask:
                    masked = static_cast<K9Atom::maskType>(masked + K9Atom::hardMask);
                    break;
/*
                case K9AtomAction::packageProvided:
                    break;
*/
                case K9AtomAction::packageUnmask:
                    masked = static_cast<K9Atom::maskType>(masked & ~(K9Atom::hardMask));
                    break;
/*
                case K9AtomAction::packageUse:
                    break;

                case K9AtomAction::packageUseForce:
                case K9AtomAction::useForce:
                    break;

                case K9AtomAction::packageUseMask:
                case K9AtomAction::useMask:
                    break;

                case K9AtomAction::packageUseStableForce:
                case K9AtomAction::useStableForce:
                    break;

                case K9AtomAction::packageUseStableMask:
                case K9AtomAction::useStableMask:
                    break;
*/
                case K9AtomAction::none:
                default:
                    break;
            }
        }
    }
}

bool ImportVDB::importInstalledPackage(QSqlQuery* query, QString category, QString package)
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
    K9Atom::maskType masked;
    status = K9Portage::UNKNOWN;
    masked = K9Atom::unsupportedMask;

    keywordList = keywords.split(' ');
    if(keywordList.contains(QString("-%1").arg(portage->arch)) || keywordList.contains("-*"))
    {
        // package is broken for this architecture
        status = K9Portage::UNKNOWN;
        masked = K9Atom::brokenMask;
    }

    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
        masked = K9Atom::notMasked;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
        masked = K9Atom::testingMask;
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

    applyConfigMasks(masked, category, packageName, slot, subslot, keywordList);

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
    query->bindValue(13, masked);
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

bool ImportVDB::importRepoPackage(QSqlQuery* query, QString category, QString packageName, QString buildsPath, QString ebuildFilePath)
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
    K9Atom::maskType masked;
    status = K9Portage::UNKNOWN;
    masked = K9Atom::unsupportedMask;

    keywordList = keywords.split(' ');
    if(keywordList.contains(QString("-%1").arg(portage->arch)) || keywordList.contains("-*"))
    {
        // package is broken for this architecture
        status = K9Portage::UNKNOWN;
        masked = K9Atom::brokenMask;
    }

    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
        masked = K9Atom::notMasked;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
        masked = K9Atom::testingMask;
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

    applyConfigMasks(masked, category, packageName, slot, subslot, keywordList);

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
    query->bindValue(13, masked);
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

bool ImportVDB::importMetaCache(QSqlQuery* query, QString category, QString packageName, QString metaCacheFilePath, QString version)
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

    portage->md5cacheReader(metaCacheFilePath);

    downloadSize = -1;
    input.setFileName(QString("/var/db/pkg/%1/%2-%3/SIZE").arg(category, packageName, portage->version.pvr));
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

    K9Atom::maskType masked;
    status = K9Portage::UNKNOWN;
    masked = K9Atom::unsupportedMask;

    keywordList = keywords.split(' ');
    if(keywordList.contains(QString("-%1").arg(portage->arch)) || keywordList.contains("-*"))
    {
        // package is broken for this architecture
        status = K9Portage::UNKNOWN;
        masked = K9Atom::brokenMask;
    }

    if(keywordList.contains(portage->arch))
    {
        status = K9Portage::STABLE;
        masked = K9Atom::notMasked;
    }
    else if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        status = K9Portage::TESTING;
        masked = K9Atom::testingMask;
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

    applyConfigMasks(masked, category, packageName, slot, subslot, keywordList);

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
    query->bindValue(13, masked);
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

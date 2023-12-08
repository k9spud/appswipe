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

#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QUuid>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QSettings>

#include "datastorage.h"
#include "main.h"

DataStorage* ds = nullptr;
int DataStorage::connectionCount = 0;

DataStorage::DataStorage()
{
    if(QSqlDatabase::drivers().isEmpty())
    {
        qDebug() << "No database drivers found";
        qDebug() << "QSQLITE database driver required. Please build the Qt SQL plugins.";
    }

    QSettings settings;
    QString defaultStoragePath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first();
    defaultStoragePath.append("/" APP_FOLDER "/");
    storageFolder = settings.value("storagePath", defaultStoragePath).toString();
    emptyDatabase = true;
}

DataStorage::~DataStorage()
{
}

bool DataStorage::isFolderWritable(QString dataFolder)
{
    QDir folder(dataFolder);
    if(!folder.exists())
    {
        if(!folder.mkdir(dataFolder))
        {
            return false;
        }
        else if(!folder.exists())
        {
            return false;
        }
    }

    QFile file(dataFolder + "testfile");
    if(file.open(QIODevice::WriteOnly) == false)
    {
        return false;
    }

    //qDebug() << "Successfully opened test file:" << file.fileName();

    file.remove();
    return true;
}

QStringList DataStorage::dataFolders()
{
    QStringList folders;
    QString folder;
    QSettings settings;

    folder = qApp->applicationDirPath() + "/../";
    if(folder.isNull() == false && folder.isEmpty() == false)
    {
        folders.append(folder);
    }
    folders.append(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first() + "/" APP_FOLDER "/");

    int i = 0;
    while(i < folders.count())
    {
        if(!folders[i].endsWith('/'))
        {
            folders[i].append("/");
        }
        folder = folders.at(i);

        if(isFolderWritable(folder) == false)
        {
            folders.removeAt(i);
        }
        else
        {
            i++;
        }
    }
    return folders;
}

QString DataStorage::openDatabase()
{
    if(storageFolder.isEmpty())
    {
        QStringList folders = dataFolders();

        if(folders.isEmpty())
        {
            qDebug() << "No data storage folder available.";
            return "";
        }

        storageFolder = folders.first();
    }

    QDir dir;
    if(dir.exists(storageFolder) == false)
    {
        dir.mkdir(storageFolder);
    }

    databaseFileName = QString(APP_FOLDER).remove('.') + ".db";
    QFileInfo fi(storageFolder + databaseFileName);

    connectionName = "DB";
    connectionName.append(QString::number(connectionCount++));
    if(fi.exists() == false)
    {
        qDebug() << "Creating settings database at:" << (storageFolder + databaseFileName);
        emptyDatabase = true;
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(storageFolder + databaseFileName);
        db.open();
        db.transaction();
        if(runSqlScript(db, ":/sql/createSettings.sql") == false)
        {
            db.rollback();
        }
        else
        {
            QSqlQuery query(db);
            query.prepare("update META set UUID=?");
            query.bindValue(0, QUuid::createUuid().toString(QUuid::WithoutBraces));
            query.exec();

            db.commit();
        }
    }
    else
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(storageFolder + databaseFileName);
        db.open();
        emptyDatabase = false;

        int schemaVersion;
        QSqlQuery query(db);
        query.exec("select SCHEMAVERSION from META");
        if(query.first() == false)
        {
            schemaVersion = -1;
        }
        else if(query.value(0).isNull())
        {
            schemaVersion = -1;
        }
        else
        {
            schemaVersion = query.value(0).toInt();
        }

        if(schemaVersion < 5)
        {
            upgradeDatabase(query, db, schemaVersion);
        }
    }

    return connectionName;
}

void DataStorage::createDatabase(QString connectionName, QString databaseFileName, QString scriptFileName)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(databaseFileName);
    db.open();

    db.transaction();
    if(runSqlScript(db, scriptFileName) == false)
    {
        db.rollback();
    }
    else
    {
        db.commit();
    }
}

bool DataStorage::upgradeDatabase(QSqlQuery& query, QSqlDatabase& db, int schemaVersion)
{
    if(db.isValid() == false)
    {
        qDebug() << "Invalid db:" << db.connectionName();
        return false;
    }

    db.transaction();

    if(schemaVersion < 5)
    {
        if(query.exec("alter table PACKAGE add column V7 int") == false)
        {
            qDebug() << "Couldn't add column PACKAGE.V7 upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }

        if(query.exec("alter table PACKAGE add column V8 int") == false)
        {
            qDebug() << "Couldn't add column PACKAGE.V8 upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }
        if(query.exec("alter table PACKAGE add column V9 int") == false)
        {
            qDebug() << "Couldn't add column PACKAGE.V9 upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }
        if(query.exec("alter table PACKAGE add column V10 int") == false)
        {
            qDebug() << "Couldn't add column PACKAGE.V10 upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }
        emptyDatabase = true;
    }

    if(schemaVersion < 4)
    {
        if(query.exec("alter table WINDOW add column TITLE text") == false)
        {
            qDebug() << "Couldn't add column WINDOW.TITLE upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }

        if(query.exec("alter table WINDOW add column STATUS integer") == false)
        {
            qDebug() << "Couldn't add column WINDOW.STATUS upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }

        if(query.exec("alter table WINDOW add column CLIP integer") == false)
        {
            qDebug() << "Couldn't add column WINDOW.CLIP upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }

        if(query.exec("alter table WINDOW add column ASK integer") == false)
        {
            qDebug() << "Couldn't add column WINDOW.ASK upgrading from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }
    }

    if(schemaVersion < 3)
    {
        if(query.exec("drop table PACKAGE") == false)
        {
            qDebug() << "Couldn't drop PACKAGE to perform upgrade from schemaVersion:" << schemaVersion;
            db.rollback();
            return false;
        }
        emptyDatabase = true;
    }

    if(runSqlScript(db, ":/sql/createSettings.sql") == false)
    {
        qDebug() << "createSettings.sql script failed during upgrade from schemaVersion" << schemaVersion << connectionName;
        db.rollback();
        return false;
    }

    int finalVersion = 5;
    query.prepare("update META set UUID=ifnull(UUID,?), SCHEMAVERSION=?");
    query.bindValue(0, QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(1, finalVersion);
    if(query.exec() == false)
    {
        qDebug() << "Update META failed during upgrade from schemaVersion" << schemaVersion << connectionName << query.executedQuery();
        db.rollback();
        return false;
    }

    qDebug() << "Database upgraded from schema version" << schemaVersion << "to" << finalVersion;

    db.commit();
    return true;
}

bool DataStorage::runSqlScript(QSqlDatabase& db, QString scriptFileName)
{
    QString data;
    QFile file(scriptFileName);
    if(!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Can't open SQL script" << scriptFileName;
        return false;
    }

    data = file.readAll();
    file.close();
    QStringList lines = data.split('\n');

    int i = 0;
    QString s;
    while(i < lines.count())
    {
        s = lines.at(i).trimmed();
        if(s.startsWith("--") || s.startsWith("//"))
        {
            lines.removeAt(i);
            continue;
        }

        i++;
    }

    QStringList statements = lines.join('\n').split(';');
    QSqlQuery query(db);
    QString sql;
    const int statementsCount = statements.count();
    for(i = 0; i < statementsCount; i++)
    {
        sql = data = statements.at(i);
        if(data.remove("\n").trimmed().isEmpty())
        {
            continue;
        }

        if(query.exec(sql) == false)
        {
            qDebug() << "SQL failed:" << data;
            return false;
        }
    }

    return true;
}

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

#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <QStringList>
#include <QSettings>
#include <QSqlDatabase>
#include <QUrl>

extern class DataStorage* ds; // main GUI thread's database connection

class DataStorage
{
public:
    DataStorage();
    ~DataStorage();

    static bool isFolderWritable(QString dataFolder);
    static QStringList dataFolders();

    QString storageFolder;
    QString databaseFileName;
    QString connectionName;

    bool emptyDatabase;

    QString openDatabase(void);
    void createDatabase(QString connectionName, QString databaseFileName, QString scriptFileName);
    bool upgradeDatabase(QSqlQuery& query, QSqlDatabase& db, int schemaVersion);
    bool runSqlScript(QSqlDatabase& db, QString scriptFileName);

private:
    static int connectionCount;
};

#endif // DATASTORAGE_H

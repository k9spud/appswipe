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

#ifndef IMPORTVDB_H
#define IMPORTVDB_H

#include <QThread>
#include <QHash>
#include <QVariant>
#include <QRegularExpression>
#include <QStringList>

extern class ImportVDB* rescan;

class QSqlQuery;
class ImportVDB
{
public:
    explicit ImportVDB();
    bool abort;

    void reloadDatabase(void);
    void reloadApp(QStringList appsList);

    bool importInstalledPackage(QSqlQuery* insertQuery, QString category, QString packagePath);
    bool importRepoPackage(QSqlQuery* insertQuery, QString category, QString packageName, QString buildsPath, QString ebuildFilePath);
    bool importMetaCache(QSqlQuery* insertQuery, QString category, QString packageName, QString metaCacheFilePath, QString version);

    int loadCategories(QStringList& categories, const QString folder);
    
protected:
};

#endif // IMPORTVDB_H

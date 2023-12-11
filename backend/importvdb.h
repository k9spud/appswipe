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

#include "k9atomlist.h"

#include <QStringList>

class QSqlQuery;
class ImportVDB
{
public:
    ImportVDB();
    bool abort;

    QStringList atoms; // allows for looking up atomId given a full atom filter string
    K9AtomList atomList; // allows for looking up parsed K9Atom objects and their configuration, using full atom filter matching
    QHash<QString, QString> makeConf; // contains /etc/portage/make.conf style environment variable settings

    void loadConfig(void);
    void readMakeConf(QString filePath);
    int readConfigFolder(QString fileFolder);
    int readProfileFolder(QString profileFolder);
    int readConfigFile(QString fileFolder, QString fileName, K9AtomAction::AtomActionType actionType);
    void applyConfigMasks(K9Atom::maskType& masked, QString category, QString package, QString slot, QString subslot, QStringList keywordList);

    void reloadDatabase(void);
    void reloadApp(QStringList appsList);

    bool importInstalledPackage(QSqlQuery* insertQuery, QString category, QString packagePath);
    bool importRepoPackage(QSqlQuery* insertQuery, QString category, QString packageName, QString buildsPath, QString ebuildFilePath);
    bool importMetaCache(QSqlQuery* insertQuery, QString category, QString packageName, QString metaCacheFilePath, QString version);

    int loadCategories(QStringList& categories, const QString folder);

protected:

private:
    QStringList profileFolders; // used to keep track of which profile folders we've already loaded so we don't get into an infinite loop
};

#endif // IMPORTVDB_H

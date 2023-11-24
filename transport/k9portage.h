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

#ifndef K9PORTAGE_H
#define K9PORTAGE_H

#include "versionstring.h"

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QHash>
#include <QRegularExpression>
#include <QSqlDatabase>

class K9Portage : public QObject
{
    Q_OBJECT

public:
    explicit K9Portage(QObject *parent = nullptr);

    QString arch;

    void setRepoFolder(QString path);
    QString repoFolder;
    QStringList repos;

    QStringList categories;

    QHash<QString, QString> vars;
    QVariant var(QString key);

    void md5cacheReader(QString fileName);

    QRegularExpression separator;
    QRegularExpression digitVersion;
    QRegularExpression alphaVersion;

    QRegularExpression dependKeywordsBasicRE;
    QRegularExpression dependKeywordsVersionRE;
    QRegularExpression dependKeywordsSlotRE;
    QRegularExpression dependKeywordsRepositoryRE;

    QRegularExpression dependLinkRE;
    QRegularExpression dependLinkSlotRE;
    QRegularExpression dependLinkAppRE;

    QString linkDependency(QString input, QString& category, QString& package);

    enum PackageStatus
    {
        UNKNOWN = 0,
        TESTING,
        STABLE
    };

signals:


protected:
};

#endif // K9PORTAGE_H

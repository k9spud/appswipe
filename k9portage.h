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

#ifndef K9PORTAGE_H
#define K9PORTAGE_H

#include "versionstring.h"

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QHash>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

class K9Portage : public QObject
{
    Q_OBJECT

public:
    explicit K9Portage(QObject *parent = nullptr);

    void setRepoFolder(QString path);
    QString repoFolder;
    QStringList repos;

    QStringList categories;

    void setVersion(QString version);
    VersionString version;
    QHash<QString, QString> vars;
    QVariant var(QString key);

    void ebuildParser(QString data);
    void parseVerCut(QString& value);
    QRegularExpression verCutSingle;
    QRegularExpression verCutRange;

    QRegularExpression separator;
    QRegularExpression digitVersion;
    QRegularExpression alphaVersion;

    QRegularExpression stringAssignment;
    QRegularExpression variableAssignment;
    QRegularExpression var_ref;

signals:

};

#endif // K9PORTAGE_H

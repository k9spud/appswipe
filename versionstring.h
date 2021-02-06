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

#ifndef VERSIONSTRING_H
#define VERSIONSTRING_H

#include <QString>
#include <QVector>

class VersionString
{
public:
    VersionString();

    void parse(QString input);

    QString cut(int index);

    // Package version and revision (if any), for example 6.3, 6.3-r1.
    QString pvr;

    // Package version (excluding revision, if any), for example 6.3.
    // It should reflect the upstream versioning scheme.
    QString pv();

    // Package revision, or r0 if no revision exists.
    QString pr();

    QVector<QString> components;
    QVector<QString> separators;
};

#endif // VERSIONSTRING_H

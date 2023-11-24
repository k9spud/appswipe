// Copyright (c) 2023, K9spud LLC.
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

#ifndef K9COMPLEXMASK_H
#define K9COMPLEXMASK_H

#include "versionstring.h"

#include <QString>

extern const QRegularExpression dependVersionRE;
extern const QRegularExpression dependSimpleRE;
extern const QRegularExpression dependBasicRE;
extern const QRegularExpression dependSlotRE;
extern const QRegularExpression dependRepositoryRE;

class K9ComplexMask
{
public:
    K9ComplexMask(const QString& mask);
    bool isMatch(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion) const;
    bool globMatch(QString glob, QString s) const;
    bool versionMatch(const VersionString& v) const;

    enum matchType
    {
        unknown = 0, Version, Basic, Slot, Repository
    };

    matchType match;
//    QString original;
    QString filter;
    QString category;
    QString package;
    QString version;
    QString repo;
    QString slot;
    QString subslot;
    VersionString vs;
};


#endif // K9COMPLEXMASK_H

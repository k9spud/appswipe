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

#ifndef K9ATOM_H
#define K9ATOM_H

#include "versionstring.h"
#include <QString>

class K9Atom
{
public:
    K9Atom(QString atom);

    bool isMatch(QString checkSlot, QString checkSubslot, const VersionString& checkVersion) const;
    bool globMatch(QString glob, QString s) const;
    bool versionMatch(const VersionString& v) const;

    enum maskType
    {
        notMasked = 0, hardMask = (1<<0), testingMask = (1<<1), unsupportedMask = (1<<2), brokenMask = (1<<3)
    };

    enum AtomType
    {
        Unknown = 0, Version, Basic, Slot, Repository
    };

    int atomId;
    AtomType type;
    QString filter;
    QString category;
    QString package;
    QString repo;
    QString slot;
    QString subslot;
    VersionString vs;
};

#endif // K9ATOM_H

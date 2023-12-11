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

#ifndef K9ATOMLIST_H
#define K9ATOMLIST_H

#include "k9atom.h"
#include "k9atomaction.h"

#include <QMultiHash>

class K9AtomList
{
public:
    K9AtomList();

    QMultiHash<QString, K9Atom> atoms; // atoms which can be looked up by "category/package"
    QList<K9Atom> globAtoms;           // atoms which have to be iterated because "category/package" contains a glob wildcard
    void appendAtom(int atomId, const QString& atomString);
    QList<int> findMatches(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion) const;

    QHash<int, QList<K9AtomAction>> atomActions; // configuration data by atomId
    void appendAtomAction(int atomId, const K9AtomAction& action);

};

#endif // K9ATOMLIST_H

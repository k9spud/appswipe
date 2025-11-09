// Copyright (c) 2023-2025, K9spud LLC.
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

#include "k9atomlist.h"

K9AtomList::K9AtomList()
{

}

void K9AtomList::appendAtom(int atomId, const QString& atomString)
{
    K9Atom atom(atomString);
    atom.atomId = atomId;
    QString key = QString("%1/%2").arg(atom.category, atom.package);

    if(key.contains('*') || key.contains('?'))
    {
        globAtoms.append(atom);
    }
    else
    {
        atoms.insert(key, atom);
    }
}

void K9AtomList::appendAtomAction(int atomId, const K9AtomAction& action)
{
    atomActions[atomId].append(action);
}

QList<int> K9AtomList::findMatches(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion) const
{
    QString key = QString("%1/%2").arg(checkCategory, checkPackage);
    QList<int> matchingIds;

    QMultiHash<QString, K9Atom>::const_iterator iter = atoms.find(key);
    while(iter != atoms.end() && iter.key() == key)
    {
        const K9Atom& atom = iter.value();
        if(atom.isMatch(checkSlot, checkSubslot, checkVersion))
        {
            matchingIds.append(atom.atomId);
        }
        iter++;
    }

    const int globAtomsCount = globAtoms.count();
    for(int i = 0; i < globAtomsCount; i++)
    {
        const K9Atom& atom = globAtoms.at(i);
        if(atom.globMatch(atom.category, checkCategory))
        {
            if(atom.globMatch(atom.package, checkPackage))
            {
                if(atom.isMatch(checkSlot, checkSubslot, checkVersion))
                {
                    matchingIds.append(atom.atomId);
                }
            }
        }
    }
    return matchingIds;
}

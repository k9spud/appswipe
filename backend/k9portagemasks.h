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

#ifndef K9PORTAGEMASKS_H
#define K9PORTAGEMASKS_H

#include "versionstring.h"
#include "k9complexmask.h"

#include <QStringList>
#include <QRegularExpression>
#include <QVector>

extern class K9PortageMasks* portageMasks;

class K9PortageMasks
{
public:
    K9PortageMasks();

    enum maskType
    {
        notMasked = 0, hardMask = (1<<0), testingMask = (1<<1), unsupportedMask = (1<<2), brokenMask = (1<<3)
    };

    void loadAll(const QStringList& repos);
    void readProfileFolder(QString profileFolder);
    void readMaskFile(QString fileName, bool unmask = false);
    void readAcceptKeywordsFile(QString fileName);

    maskType isMasked(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion, const QString& keywords) const;

    QStringList masks; // no globs, no version comparisons, just 'category/package'
    QStringList masksExactVersion; // no globs or greater/less thans, just 'category/package-version'
    QVector<K9ComplexMask> masksComplex; // everything else

    QStringList unmasks; // no globs, no version comparisons, just 'category/package'
    QStringList unmasksExactVersion; // no globs or greater/less thans, just 'category/package-version'
    QVector<K9ComplexMask> unmasksComplex; // everything else

    QStringList acceptKeywords; // no globs, no version comparisons, just 'category/package'
    QStringList acceptKeywordsAllowed; // architectures (keywords) that are allowed by this accept_keywords rule

    QVector<K9ComplexMask> acceptKeywordsComplex; // everything else
    QStringList acceptKeywordsComplexAllowed; // architectures (keywords) that are allowed by this accept_keywords rule

protected:

private:
    QStringList profileFolders; // used to keep track of which profile folders we've already loaded so we don't get into an infinite loop
};

#endif // K9PORTAGEMASKS_H

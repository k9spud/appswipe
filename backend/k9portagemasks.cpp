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

#include "k9portagemasks.h"
#include "k9portage.h"
#include "versionstring.h"
#include "globals.h"

#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QRegularExpressionMatch>

K9PortageMasks* portageMasks = nullptr;

K9PortageMasks::K9PortageMasks()
{
}

void K9PortageMasks::loadAll(const QStringList& repos)
{
    QString repo;
    QFileInfo fi;
    const int repoCount = repos.count();
    for(int i = 0; i < repoCount; i++)
    {
        repo = repos.at(i);
        readMaskFile(QString("%1profiles/package.mask").arg(repo));
        readMaskFile(QString("%1profiles/package.unmask").arg(repo), true /* unmask */);
        readAcceptKeywordsFile(QString("%1profiles/package.accept_keywords").arg(repo));
    }

    fi.setFile("/etc/portage/make.profile");
    if(fi.isDir())
    {
        readProfileFolder(fi.canonicalFilePath());
    }

    readMaskFile("/etc/portage/package.mask");
    readMaskFile("/etc/portage/package.unmask", true /* unmask */);
    readAcceptKeywordsFile("/etc/portage/package.accept_keywords");

    profileFolders.clear();
/*
    output << "total simple mask rules:" << masks.count() << Qt::endl;
    for(int i = 0; i < masks.count(); i++)
    {
        output << "  " << masks.at(i) << Qt::endl;
    }

    output << "total exact version mask rules:" << masksExactVersion.count() << Qt::endl;
    for(int i = 0; i < masksExactVersion.count(); i++)
    {
        output << "  " << masksExactVersion.at(i) << Qt::endl;
    }

    output << "total complex mask rules:" << masksComplex.count() << Qt::endl;
    for(int i = 0; i < masksComplex.count(); i++)
    {
        output << "  " << masksComplex.at(i).original << Qt::endl;
    }

    output << "total simple unmask rules:" << unmasks.count() << Qt::endl;
    output << "total exact version unmask rules:" << unmasksExactVersion.count() << Qt::endl;
    output << "total complex unmask rules:" << unmasksComplex.count() << Qt::endl;

    output << "total simple accept_keywords rules:" << acceptKeywords.count() << Qt::endl;
    for(int i = 0; i < acceptKeywords.count(); i++)
    {
        output << "  " << acceptKeywords.at(i) << " allow[" << acceptKeywordsAllowed.at(i) << "]" << Qt::endl;
    }

    output << "total complex accept_keywords rules:" << acceptKeywordsComplex.count() << Qt::endl;
    for(int i = 0; i < acceptKeywordsComplex.count(); i++)
    {
        output << "  " << acceptKeywordsComplex.at(i).original << " allow[" << acceptKeywordsComplexAllowed.at(i) << "]" << Qt::endl;
    }
*/
}

void K9PortageMasks::readProfileFolder(QString profileFolder)
{
    if(profileFolders.contains(profileFolder))
    {
        // don't get stuck in an infinite loop of repeating profile folders
        return;
    }
    profileFolders.append(profileFolder);

    readMaskFile(QString("%1/package.mask").arg(profileFolder));

    int i;
    QString s = QString("%1/parent").arg(profileFolder);
    QFileInfo fi;
    QFile input;
    input.setFileName(s);
    if(input.exists())
    {
        if(input.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            s = input.readAll();
            input.close();
            QStringList lines = s.split('\n');
            const int lineCount = lines.count();
            for(i = 0; i < lineCount; i++)
            {
                fi.setFile(QString("%1/%2").arg(profileFolder, lines.at(i)));
                if(fi.isDir())
                {
                    readProfileFolder(fi.canonicalFilePath());
                }
            }
        }
    }
}

// refer to https://web.archive.org/web/20201112040501/https://wiki.gentoo.org/wiki/Version_specifier
// for DEPEND atom syntax
void K9PortageMasks::readMaskFile(QString fileName, bool unmask)
{
    QString s;
    int i;
    QFileInfo fi;
    fi.setFile(fileName);
    if(fi.isDir())
    {
        QDir dir;
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setPath(fi.absoluteFilePath());

        QStringList files = dir.entryList();
        const int fileCount = files.count();
        for(i = 0; i < fileCount; i++)
        {
            readMaskFile(QString("%1/%2").arg(dir.path(), files.at(i)), unmask);
        }
        return;
    }

    if(fi.exists() == false || fi.isFile() == false)
    {
        return;
    }

    QFile input;
    input.setFileName(fileName);
    if(!input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    s = input.readAll();
    input.close();
    QRegularExpressionMatch match;

    QStringList lines = s.split('\n');
    const int lineCount = lines.count();
    for(int line = 0; line < lineCount; line++)
    {
        s = lines.at(line).trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        if(s.startsWith('-'))
        {
            s = s.mid(1);
            int i = masks.indexOf(s);
            if(i != -1)
            {
                masks.removeAt(i);
            }
            continue;
        }

        if(s.contains("*") == false && s.contains("?") == false)
        {
            if(dependSimpleRE.match(s).hasMatch())
            {
                if(unmask)
                {
                    unmasks.append(s);
                    continue;
                }
                else
                {
                    masks.append(s);
                    continue;
                }
            }

            match = dependVersionRE.match(s);
            if(match.hasMatch() && match.captured(1) /* filter operator */ == "=")
            {
                if(unmask)
                {
                    unmasksExactVersion.append(s.mid(1));
                    continue;
                }
                else
                {
                    masksExactVersion.append(s.mid(1));
                    continue;
                }
            }
        }

        if(unmask)
        {
            unmasksComplex.append(K9ComplexMask(s));
        }
        else
        {
            masksComplex.append(K9ComplexMask(s));
        }
    }
}

void K9PortageMasks::readAcceptKeywordsFile(QString fileName)
{
    QFileInfo fi;
    int i;
    fi.setFile(fileName);
    if(fi.isDir())
    {
        QDir dir;
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setPath(fi.absoluteFilePath());
        QStringList files = dir.entryList();
        const int fileCount = files.count();
        for(i = 0; i < fileCount; i++)
        {
            readAcceptKeywordsFile(QString("%1/%2").arg(dir.path(), files.at(i)));
        }
        return;
    }

    if(fi.exists() == false || fi.isFile() == false)
    {
        return;
    }

    QFile input;
    input.setFileName(fileName);
    if(!input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QString s = input.readAll();
    input.close();
    QStringList lines = s.split('\n');
    QString allow;
    QRegularExpressionMatch match;
    const int lineCount = lines.count();
    for(int line = 0; line < lineCount; line++)
    {
        s = lines.at(line).trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        if(s.startsWith('-'))
        {
            s = s.mid(1);
            i = masks.indexOf(s);
            if(i != -1)
            {
                masks.removeAt(i);
            }
            continue;
        }

        i = s.indexOf(' ');
        if(i < 0)
        {
            allow = QString("~%1").arg(portage->arch);
        }
        else
        {
            allow = s.mid(i+1);
            s = s.left(i);
        }

        if(s.contains("*") == false && s.contains("?") == false && dependSimpleRE.match(s).hasMatch())
        {
            acceptKeywords.append(s);
            acceptKeywordsAllowed.append(allow);
        }
        else
        {
            acceptKeywordsComplex.append(K9ComplexMask(s));
            acceptKeywordsComplexAllowed.append(allow);
        }
    }
}

K9PortageMasks::maskType K9PortageMasks::isMasked(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion, const QString& keywords) const
{
    int i, j, k;

    maskType maskResult = notMasked;
    QString atom = QString("%1/%2").arg(checkCategory, checkPackage);
    QString atomVersion = atom;
    atomVersion.append(QString("-%1").arg(checkVersion.pvr));
    if(masks.contains(atom) || masksExactVersion.contains(atomVersion))
    {
        maskResult = hardMask;
    }
    else
    {
        const int masksComplexCount = masksComplex.count();
        for(i = 0; i < masksComplexCount; i++)
        {
            if(masksComplex.at(i).isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion))
            {
                maskResult = hardMask;
            }
        }
    }

    if(maskResult) // don't bother processing package.unmask rules if this atom isn't masked
    {
        if(unmasks.contains(atom) || unmasksExactVersion.contains(atomVersion))
        {
            maskResult = notMasked;
        }
        else
        {
            const int unmasksComplexCount = unmasksComplex.count();
            for(i = 0; i < unmasksComplexCount; i++)
            {
                if(unmasksComplex.at(i).isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion))
                {
                    maskResult = notMasked;
                }
            }
        }
    }

    QStringList keywordList = keywords.split(' ');
    if(keywordList.contains(portage->arch))
    {
        // Package is visible if it is stable on the current system's architecture.
        return maskResult;
    }

    QString s;
    int allowedCount;
    const int keywordCount = keywordList.count();
    const int acceptKeywordCount = acceptKeywords.count();
    for(i = 0; i < acceptKeywordCount; i++)
    {
        i = acceptKeywords.indexOf(atom, i);
        if(i == -1)
        {
            // couldn't find atom in simple acceptKeywords list, try the complex list instead.
            break;
        }

        s = acceptKeywordsAllowed.at(i);
        if(s.isEmpty())
        {
            // Lines without any accept_keywords imply unstable host arch.
            if(keywordList.contains(QString("~%1").arg(portage->arch)))
            {
                return maskResult;
            }
            continue;
        }

        QStringList allowed = s.split(' ', Qt::SkipEmptyParts);
        allowedCount = allowed.count();
        for(k = 0; k < allowedCount; k++)
        {
            s = allowed.at(k);
            if(s == "**")
            {
                // Package is always visible (KEYWORDS are ignored completely)
                return maskResult;
            }

            if(s == "*")
            {
                // Package is visible if it is stable on any architecture.
                for(j = 0; j < keywordCount; j++)
                {
                    s = keywordList.at(j);
                    if(s.isEmpty() == false && s.startsWith("~") == false && s.startsWith("-") == false)
                    {
                        return maskResult;
                    }
                }
            }
            else if(s == "~*")
            {
                // Package is visible if it is in testing on any architecture.
                for(j = 0; j < keywordCount; j++)
                {
                    if(keywordList.at(j).startsWith("~") == true)
                    {
                        return maskResult;
                    }
                }
            }
            else
            {
                // Package is visible if keywords match package.accept_keywords
                if(keywordList.contains(s))
                {
                    return maskResult;
                }
            }
        }
    }

    QStringList allowed;
    for(i = 0; i < acceptKeywordsComplex.count(); i++)
    {
        if(acceptKeywordsComplex.at(i).isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion) == false)
        {
            continue;
        }

        allowed = acceptKeywordsComplexAllowed.at(i).split(' ');
        allowedCount = allowed.count();
        for(k = 0; k < allowedCount; k++)
        {
            s = allowed.at(k);
            if(s == "**")
            {
                // Package is always visible (KEYWORDS are ignored completely)
                return maskResult;
            }

            if(s == "*")
            {
                // Package is visible if it is stable on any architecture.
                for(j = 0; j < keywordCount; j++)
                {
                    s = keywordList.at(j);
                    if(s.isEmpty() == false && s.startsWith("~") == false && s.startsWith("-") == false)
                    {
                        return maskResult;
                    }
                }
            }
            else if(s == "~*")
            {
                // Package is visible if it is in testing on any architecture.
                for(j = 0; j < keywordCount; j++)
                {
                    s = keywordList.at(j);
                    if(s.isEmpty() == false && s.startsWith("-") == false && s.startsWith("~") == true)
                    {
                        return maskResult;
                    }
                }
            }
            else
            {
                // Package is visible if keywords match package.accept_keywords
                if(keywordList.contains(s))
                {
                    return maskResult;
                }
            }
        }
    }

    // couldn't find any rules to accept this package's keywords.
    if(keywordList.contains(QString("~%1").arg(portage->arch)))
    {
        // package is in testing for this architecture
        return static_cast<maskType>(maskResult | testingMask);
    }

    if(keywordList.contains(QString("-%1").arg(portage->arch)) || keywordList.contains("-*"))
    {
        // package is broken for this architecture
        return static_cast<maskType>(maskResult | brokenMask);
    }

    return static_cast<maskType>(maskResult | unsupportedMask);
}

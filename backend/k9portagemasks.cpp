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
    QString s;
    QFileInfo fi;
    QDir dir;
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    foreach(QString repo, repos)
    {
        s = QString("%1profiles/package.mask").arg(repo);
        readMaskFile(s);

        s = QString("%1profiles/package.unmask").arg(repo);
        readMaskFile(s, true /* unmask */);

        s = QString("%1profiles/package.accept_keywords").arg(repo);
        readAcceptKeywordsFile(s);
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

    QString s = QString("%1/parent").arg(profileFolder);
    QFileInfo fi;
    fi.setFile(s);
    if(fi.isFile())
    {
        QFile input;
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString wholeFile = input.readAll();
            input.close();
            QStringList lines = wholeFile.split('\n');
            foreach(QString parent, lines)
            {
                parent = QString("%1/%2").arg(profileFolder, parent);
                fi.setFile(parent);
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
    QFileInfo fi;
    fi.setFile(fileName);
    if(fi.isDir())
    {
        QDir dir;
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setPath(fi.absoluteFilePath());
        foreach(QString maskFile, dir.entryList())
        {
            readMaskFile(QString("%1/%2").arg(dir.path(), maskFile), unmask);
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

    QString wholeFile = input.readAll();
    input.close();
    QStringList lines = wholeFile.split('\n');
//    QString comment;
    QString s;
    QRegularExpressionMatch match;
    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty())
        {
//            comment.clear();
            continue;
        }

        if(s.startsWith('#'))
        {
/*            if(comment.isEmpty() == false)
            {
                comment.append("\n");
            }
            comment.append(s.mid(1).trimmed());*/
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
    fi.setFile(fileName);
    if(fi.isDir())
    {
        QDir dir;
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setPath(fi.absoluteFilePath());
        foreach(QString maskFile, dir.entryList())
        {
            readAcceptKeywordsFile(QString("%1/%2").arg(dir.path(), maskFile));
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

    QString wholeFile = input.readAll();
    input.close();
    QStringList lines = wholeFile.split('\n');
//    QString comment;
    QString s;
    QString allow;
    QRegularExpressionMatch match;
    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty())
        {
//            comment.clear();
            continue;
        }

        if(s.startsWith('#'))
        {
/*            if(comment.isEmpty() == false)
            {
                comment.append("\n");
            }
            comment.append(s.mid(1).trimmed());*/
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

        int i = s.indexOf(' ');
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
        foreach(K9ComplexMask k, masksComplex)
        {
            if(k.isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion))
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
            foreach(K9ComplexMask k, unmasksComplex)
            {
                if(k.isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion))
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

    int i;
    QString s;
    for(i = 0; i < acceptKeywords.count(); i++)
    {
        i = acceptKeywords.indexOf(atom, i);
        if(i == -1)
        {
            // couldn't find atom in either simple acceptKeywords lists, try the complex list instead.
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
        foreach(s, allowed)
        {
            if(s == "**")
            {
                // Package is always visible (KEYWORDS are ignored completely)
                return maskResult;
            }

            if(s == "*")
            {
                // Package is visible if it is stable on any architecture.
                foreach(s, keywordList)
                {
                    if(s.isEmpty() == false && s.startsWith("~") == false && s.startsWith("-") == false)
                    {
                        return maskResult;
                    }
                }
            }
            else if(s == "~*")
            {
                // Package is visible if it is in testing on any architecture.
                foreach(s, keywordList)
                {
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

    for(i = 0; i < acceptKeywordsComplex.count(); i++)
    {
        K9ComplexMask k = acceptKeywordsComplex.at(i);
        if(k.isMatch(checkCategory, checkPackage, checkSlot, checkSubslot, checkVersion) == false)
        {
            continue;
        }

        QStringList allowed = acceptKeywordsComplexAllowed.at(i).split(' ');
        foreach(s, allowed)
        {
            if(s == "**")
            {
                // Package is always visible (KEYWORDS are ignored completely)
                return maskResult;
            }

            if(s == "*")
            {
                // Package is visible if it is stable on any architecture.
                foreach(s, keywordList)
                {
                    if(s.isEmpty() == false && s.startsWith("~") == false && s.startsWith("-") == false)
                    {
                        return maskResult;
                    }
                }
            }
            else if(s == "~*")
            {
                // Package is visible if it is in testing on any architecture.
                foreach(s, keywordList)
                {
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

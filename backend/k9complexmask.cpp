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

#include "k9complexmask.h"

#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

// refer to https://web.archive.org/web/20201112040501/https://wiki.gentoo.org/wiki/Version_specifier
// for DEPEND atom syntax
// https://dev.gentoo.org/~ulm/pms/head/pms.html#section-8.3

const QRegularExpression dependVersionRE =
        QRegularExpression(QStringLiteral("^(~|=|>=|<=|>|<)"                // operand
                                        "([^/]+)"                           // category
                                        "/"
                                        "([^:]+)"                           // package name
                                        "-([0-9\\*][0-9\\-\\.A-z\\*]*)$")); // version
const QRegularExpression dependSimpleRE =
        QRegularExpression(QStringLiteral("^([A-Za-z0-9_][A-Za-z0-9\\+_\\.-]*)" // category
                                          "/"
                                          "([A-Za-z0-9_][A-Za-z0-9\\+_-]*)+$"));
const QRegularExpression dependBasicRE =
        QRegularExpression(QStringLiteral("([^~=><][^/]*)/([^:\\n]+)$"));
const QRegularExpression dependSlotRE =
        QRegularExpression(QStringLiteral("([^~=><][^/]*)/([^:\\n]+):([^:\\n]+)"));
const QRegularExpression dependRepositoryRE =
        QRegularExpression(QStringLiteral("(~|=|>=|<=|>|<)([^/]+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n]+)"));

K9ComplexMask::K9ComplexMask(const QString& mask)
{
//    original = mask;
    QRegularExpressionMatch regmatch;

    regmatch = dependVersionRE.match(mask);
    if(regmatch.hasMatch())
    {
        match = K9ComplexMask::Version;
        filter = regmatch.captured(1);
        category = regmatch.captured(2);
        package = regmatch.captured(3);
        version = regmatch.captured(4);
        vs.parse(version);
        return;
    }

    regmatch = dependBasicRE.match(mask);
    if(regmatch.hasMatch())
    {
        match = K9ComplexMask::Basic;
        category = regmatch.captured(1);
        package = regmatch.captured(2);
        return;
    }

    regmatch = dependSlotRE.match(mask);
    if(regmatch.hasMatch())
    {
        match = K9ComplexMask::Slot;
        category = regmatch.captured(1);
        package = regmatch.captured(2);
        slot = regmatch.captured(3);
        if(slot.contains('/'))
        {
            int ix = slot.indexOf('/');
            subslot = slot.mid(ix + 1);
            slot = slot.left(ix);
        }
        return;
    }

    regmatch = dependRepositoryRE.match(mask);
    if(regmatch.hasMatch())
    {
        match = K9ComplexMask::Repository;
        filter = regmatch.captured(1);
        category = regmatch.captured(2);
        package = regmatch.captured(3);
        version = regmatch.captured(4);
        vs.parse(version);
        slot = regmatch.captured(5);
        if(slot.contains('/'))
        {
            int ix = slot.indexOf('/');
            subslot = slot.mid(ix + 1);
            slot = slot.left(ix);
        }
        return;
    }

    match = K9ComplexMask::unknown;
}

bool K9ComplexMask::globMatch(QString glob, QString s) const
{
    if(glob == "*")
    {
        return true;
    }

    int i = glob.indexOf('*');
    int q = glob.indexOf('?');
    if(q == -1)
    {
        if(i == -1)
        {
            return glob == s;
        }

        if(i == (glob.count() - 1))
        {
            glob.chop(1);
            return s.startsWith(glob);
        }
    }

    glob.replace("*", ".*");
    glob.replace('?', '.');

    QRegularExpression regex(glob);
    QRegularExpressionMatch match;
    match = regex.match(s);
    return match.hasMatch();
}

bool K9ComplexMask::versionMatch(const VersionString& vb) const
{
    if(filter == "=" && version.contains("*") == false)
    {
        return version == vb.pvr;
    }

    int i;
    qint64 a;
    qint64 b;

    if(filter == "=")
    {
        if(version.startsWith('*') && version.endsWith('*'))
        {
            return vb.pvr.contains(version.mid(1, version.length() - 2));
        }

        for(i = 0; i < vs.vi.count() && i < vb.vi.count(); i++)
        {
            a = vs.vi.at(i);
            b = vb.vi.at(i);
            if(vs.vx.at(i).endsWith('*'))
            {
                if(a == b)
                {
                    return true;
                }
                return false;
            }

            if(a != b)
            {
                return false;
            }
        }

        // both versions are equal
        return true;
    }

    if(filter == "<=")
    {
        for(i = 0; i < vs.vi.count() && i < vb.vi.count(); i++)
        {
            a = vs.vi.at(i);
            b = vb.vi.at(i);
            if(b > a)
            {
                return false;
            }

            if(b < a)
            {
                return true;
            }
        }

        // both versions are equal
        return true;
    }

    if(filter == ">=")
    {
        for(i = 0; i < vs.vi.count() && i < vb.vi.count(); i++)
        {
            a = vs.vi.at(i);
            b = vb.vi.at(i);
            if(b < a)
            {
                return false;
            }

            if(b > a)
            {
                return true;
            }
        }

        // both versions are equal
        return true;
    }


    if(filter == "<")
    {
        for(i = 0; i < vs.vi.count() && i < vb.vi.count(); i++)
        {
            a = vs.vi.at(i);
            b = vb.vi.at(i);
            if(b > a)
            {
                return false;
            }

            if(b < a)
            {
                return true;
            }
        }

        // both versions are equal
        return false;
    }

    if(filter == ">")
    {
        for(i = 0; i < vs.vi.count() && i < vb.vi.count(); i++)
        {
            a = vs.vi.at(i);
            b = vb.vi.at(i);
            if(b < a)
            {
                return false;
            }

            if(b > a)
            {
                return true;
            }
        }

        // both versions are equal
        return false;
    }

    if(filter == "~")
    {
        for(i = 0; i < (MAXVX-1) && i < vs.vx.count() && i < vb.vx.count(); i++)
        {
            if(vs.vx.at(i) != vb.vx.at(i))
            {
                return false;
            }
        }

        // both versions are equal (while ignoring any -rX revision number differences)
        return true;
    }

    qDebug() << "Unknown filter op:" << filter << "App Version:" << vb.pvr << "Match Version" << version;
    return false;
}

bool K9ComplexMask::isMatch(const QString& checkCategory, const QString& checkPackage, const QString& checkSlot, const QString& checkSubslot, const VersionString& checkVersion) const
{
    switch(match)
    {
        case K9ComplexMask::Version:
            if(globMatch(category, checkCategory))
            {
                if(globMatch(package, checkPackage))
                {
                    if(versionMatch(checkVersion))
                    {
//                        qDebug() << "masked:" << s << checkCategory <<  checkPackage << checkVersion.pvr << checkSlot << checkSubslot;
                        return true;
                    }
                }
            }
            break;

        case K9ComplexMask::Basic:
            if(globMatch(category, checkCategory))
            {
                if(globMatch(package, checkPackage))
                {
//                    qDebug() << "masked:" << s << checkCategory <<  checkPackage << checkVersion.pvr << checkSlot << checkSubslot;
                    return true;
                }
            }
            break;

        case K9ComplexMask::Slot:
            if(globMatch(category, checkCategory))
            {
                if(globMatch(package, checkPackage))
                {
                    if(globMatch(slot, checkSlot))
                    {
                        if(subslot.isEmpty() || globMatch(subslot, checkSubslot))
                        {
                            //qDebug() << "masked:" << s << checkCategory <<  checkPackage << checkVersion.pvr << checkSlot << checkSubslot;
                            return true;
                        }
                    }
                }
            }
            break;

        case K9ComplexMask::Repository:
            if(globMatch(category, checkCategory))
            {
                if(globMatch(package, checkPackage))
                {
                    if(versionMatch(checkVersion))
                    {
                        if(globMatch(slot, checkSlot))
                        {
                            if(subslot.isEmpty() || globMatch(subslot, checkSubslot))
                            {
                                //qDebug() << "masked:" << s << checkCategory <<  checkPackage << checkVersion.pvr << checkSlot << checkSubslot;
                                return true;
                            }
                        }
                    }
                }
            }
            break;

        default:
            break;
    }

    return false;
}

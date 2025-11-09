// Copyright (c) 2021-2025, K9spud LLC.
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

#include "versionstring.h"
#include "k9portage.h"
#include "globals.h"

#include <QRegularExpression>
#include <QDebug>

/*
Refer to https://devmanual.gentoo.org/ebuild-writing/file-format/index.html
for latest information regarding version number comparisons:

_alpha  Alpha release (earliest)
_beta   Beta release
_pre    Pre release
_rc     Release candidate
(no suffix) Normal release
_p      Patch release (most recent)

To get Portage to compare two versions:

python -c "from portage.versions import vercmp ; print(vercmp('3.12.0', '3.12.0_rc1'))"

https://dev.gentoo.org/~ulm/pms/head/pms.html#section-3.3 more details in the PMS
^ Package Manager Specification

you can also: source /usr/lib/portage/pythonXXX/eapi7-ver-funcs.sh and use ver_test
*/

/*
See https://mgorny.pl/articles/the-ultimate-guide-to-eapi-7.html#version-manipulation-and-comparison-commands
for ver_cut explanation.
*/

VersionString::VersionString()
{
}

void VersionString::parse(QString input)
{
    QRegularExpressionMatch match;
    QString s;
    pvr = input;
    int position = 0;
    int oldPosition = -1;
    components.clear();
    vx.clear();
    vx.reserve(MAXVX);
    while(position < input.size() && position != oldPosition)
    {
        oldPosition = position;

#if QT_VERSION < 0x060000
        match = portage->separator.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
#else
        match = portage->separator.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
#endif
        if(match.hasMatch())
        {
            position = match.capturedEnd();
        }

#if QT_VERSION < 0x060000
        match = portage->digitVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
#else
        match = portage->digitVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
#endif
        if(match.hasMatch())
        {
            s = match.captured(1);
            components.append(s);
            vx.append(s);
            position = match.capturedEnd();
        }
        else
        {
#if QT_VERSION < 0x060000
            match = portage->alphaVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
#else
            match = portage->alphaVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
#endif
            if(match.hasMatch())
            {
                s = match.captured(1);
                components.append(s);
                vx.append(s);
                position = match.capturedEnd();
            }
            else
            {
                break; // this shouldn't happen
            }
        }
    }

    QString revision = "0";
    QRegularExpression digitsRegEx("^\\d+$");
    for(int i = 0; i < vx.count(); i++)
    {
        s = vx.at(i);

        // alpha, beta, pre, and rc release numbers need to be treated as negative numbers
        // so that we can get the SQL sort order functionality to work right.
        if(s == "alpha" && (i+1) < vx.count())
        {
            vx[i] = QString::number(-40000 + vx.at(i+1).toInt());
            vx.removeAt(i+1);
        }
        else if(s == "beta" && (i+1) < vx.count())
        {
            vx[i] = QString::number(-30000 + vx.at(i+1).toInt());
            vx.removeAt(i+1);
        }
        else if(s == "pre" && (i+1) < vx.count())
        {
            vx[i] = QString::number(-20000 + vx.at(i+1).toInt());
            vx.removeAt(i+1);
        }
        else if(s == "rc" && (i+1) < vx.count())
        {
            vx[i] = QString::number(-10000 + vx.at(i+1).toInt());
            vx.removeAt(i+1);
        }
        else if(s == "r" && (i+1) < vx.count())
        {
            // 'r' revision numbers should be treated as a positive number, but we want to strip out the
            // letter 'r' to save database Vx columns and we want the revision number to be the very least
            // significant tuple rather than being mixed in somewhere else.

            revision = vx.at(i+1);
            vx.removeAt(i);
            vx.removeAt(i);
        }
        else if(s == "p" && (i+1) < vx.count() && vx.at(i+1).contains(digitsRegEx))
        {
            // 'p' patch releases should be treated as a positive number, but we want to strip out the
            // letter 'p' to save database Vx columns for the really crazy long version numbers some packages have.

            vx[i] = vx.at(i+1);
            vx.removeAt(i+1);
        }
    }

    // if any of the Vx columns are left as a NULL, it would make the SQL code much more complicated.
    // to avoid that, we need to pad out any unused tuples to being a "0" instead.
    for(int i = 0; i < (MAXVX - 1); i++)
    {
        if(i >= vx.count())
        {
            vx.append("0");
        }

        s = vx.at(i);
        if(s.isEmpty())
        {
            vx[i] = "0";
        }
    }

    // revision number always as least significant Vx column (V10). If no revision number specified,
    // it will be defaulted to "0" instead of NULL (code above during declaration of 'revision' variable).
    vx.append(revision);

    qint64 i;
    bool ok;
    vi.clear();
    vi.reserve(MAXVX);
    const int vxCount = vx.count();
    for(int j = 0; j < vxCount; j++)
    {
        s = vx.at(j);
        i = s.toLongLong(&ok);
        if(ok == false)
        {
//            qDebug() << "couldn't convert:" << s;
            i = 0;
        }
        vi.append(i);
    }
}

bool VersionString::match(QString filter, QString version2) const
{
    if(filter == "=" && version2.contains("*") == false)
    {
        return version2 == pvr;
    }

    VersionString vb;
    vb.parse(version2);
    int a;
    int b;
    int i;

    if(filter == "=")
    {
        for(i = 0; i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(vb.vx.at(i).endsWith('*'))
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
        for(i = 0; i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(a > b)
            {
                return false;
            }

            if(a < b)
            {
                return true;
            }
        }

        // both versions are equal
        return true;
    }

    if(filter == ">=")
    {
        for(i = 0; i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(a < b)
            {
                return false;
            }

            if(a > b)
            {
                return true;
            }
        }

        // both versions are equal
        return true;
    }


    if(filter == "<")
    {
        for(i = 0; i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(a > b)
            {
                return false;
            }

            if(a < b)
            {
                return true;
            }
        }

        // both versions are equal
        return false;
    }

    if(filter == ">")
    {
        for(i = 0; i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(a < b)
            {
                return false;
            }

            if(a > b)
            {
                return true;
            }
        }

        // both versions are equal
        return false;
    }

    if(filter == "~")
    {
        for(i = 0; i < (MAXVX-1) && i < vb.vx.count() && i < vx.count(); i++)
        {
            a = vi.at(i);
            b = vb.vi.at(i);
            if(a != b)
            {
                return false;
            }
        }

        // both versions are equal (while ignoring any -rX revision number differences)
        return true;
    }

    qDebug() << "Unknown filter op:" << filter << "App Version:" << pvr << "Match Version" << version2;
    return false;
}

QString VersionString::cut(int index)
{
    if(index >= 0 && index < components.count())
    {
        return components.at(index);
    }

    return QString();
}

QString VersionString::cutInternalVx(int index)
{
    if(index >= 0 && index < vx.count())
    {
        return vx.at(index);
    }

    return QString();
}

QString VersionString::revision()
{
    int i = pvr.indexOf("-r");
    if(i >= 0)
    {
        return pvr.mid(i + 2);
    }

    return "0";
}

QString VersionString::pv()
{
    int i = pvr.indexOf("-r");
    if(i >= 0)
    {
        return pvr.left(i);
    }

    return pvr;
}

QString VersionString::pr()
{
    int i = pvr.indexOf("-r");
    if(i >= 0)
    {
        return pvr.mid(i + 1);
    }

    return "r0";
}

void VersionString::greaterThanEqualToSQL(QString& clauses)
{
    int count = vx.count();
    QString s;
    if(count == 1)
    {
        s = QString("(V1 >= %1)").arg(vx.at(0));
    }
    else if(count >= 2)
    {
        s = QString("(V1 > %1 or (V1 = %1 and (%2)))").arg(escapeSql(vx.at(0)), greaterThanEqualToSQL(1));
    }

    if(s.isEmpty() == false)
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }
        clauses.append(s);
    }
}

QString VersionString::greaterThanEqualToSQL(int i)
{
    int count = vx.count();
    if(i >= count)
    {
        return "";
    }

    QString s;

    if(i + 2 <= count && i + 1 < MAXVX)
    {
        s = QString("V%1 > %2 or (V%1 = %2 and (%3))").arg(i+1).arg(escapeSql(vx.at(i)), greaterThanEqualToSQL(i+1));
    }
    else
    {
        s = QString("V%1 >= %2").arg(i+1).arg(escapeSql(vx.at(i)));
    }
    return s;
}

void VersionString::lessThanEqualToSQL(QString& clauses)
{
    int count = vx.count();
    QString s;
    if(count == 1)
    {
        s = QString("(V1 <= %1)").arg(vx.at(0));
    }
    else if(count >= 2)
    {
        s = QString("(V1 < %1 or (V1 = %1 and (%2)))").arg(escapeSql(vx.at(0)), lessThanEqualToSQL(1));
    }

    if(s.isEmpty() == false)
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }
        clauses.append(s);
    }
}

QString VersionString::lessThanEqualToSQL(int i)
{
    int count = vx.count();
    if(i >= count)
    {
        return "";
    }

    QString s;

    if(i + 2 <= count && i + 1 < MAXVX)
    {
        s = QString("V%1 < %2 or (V%1 = %2 and (%3))").arg(i+1).arg(escapeSql(vx.at(i)), lessThanEqualToSQL(i+1));
    }
    else
    {
        s = QString("V%1 <= %2").arg(i+1).arg(escapeSql(vx.at(i)));
    }
    return s;
}

QString VersionString::greaterThanSQL(int i)
{
    int count = vx.count();
    if(i >= count)
    {
        return "";
    }

    QString s;

    if(i + 2 <= count && i + 1 < MAXVX)
    {
        s = QString("V%1 > %2 or (V%1 = %2 and (%3))").arg(i+1).arg(escapeSql(vx.at(i)), greaterThanSQL(i+1));
    }
    else
    {
        s = QString("V%1 > %2").arg(i+1).arg(escapeSql(vx.at(i)));
    }
    return s;
}

void  VersionString::greaterThanSQL(QString& clauses)
{
    int count = vx.count();
    QString s;
    if(count == 1)
    {
        s = QString("(V1 > %1)").arg(vx.at(0));
    }
    else if(count >= 2)
    {
        s = QString("(V1 > %1 or (V1 = %1 and (%2)))").arg(escapeSql(vx.at(0)), greaterThanSQL(1));
    }

    if(s.isEmpty() == false)
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }
        clauses.append(s);
    }

}

QString VersionString::lessThanSQL(int i)
{
    int count = vx.count();
    if(i >= count)
    {
        return "";
    }

    QString s;

    if(i + 2 <= count && i + 1 < MAXVX)
    {
        s = QString("V%1 < %2 or (V%1 = %2 and (%3))").arg(i+1).arg(escapeSql(vx.at(i)), lessThanSQL(i+1));
    }
    else
    {
        s = QString("V%1 < %2").arg(i+1).arg(escapeSql(vx.at(i)));
    }
    return s;
}

void VersionString::lessThanSQL(QString& clauses)
{
    int count = vx.count();
    QString s;
    if(count == 1)
    {
        s = QString("(V1 < %1)").arg(vx.at(0));
    }
    else if(count >= 2)
    {
        s = QString("(V1 < %1 or (V1 = %1 and (%2)))").arg(escapeSql(vx.at(0)), lessThanSQL(1));
    }

    if(s.isEmpty() == false)
    {
        if(clauses.isEmpty() == false)
        {
            clauses.append(" and ");
        }
        clauses.append(s);
    }
}

QString VersionString::escapeSql(QString s)
{
    bool ok;
    s.toInt(&ok);
    if(ok)
    {
        return s;
    }

    s.replace("'", "''");
    return QString("'%1'").arg(s);
}

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

#include "versionstring.h"
#include "k9portage.h"
#include "globals.h"

#include <QRegularExpression>
#include <QDebug>

VersionString::VersionString()
{
}

void VersionString::parse(QString input)
{
    QRegularExpressionMatch match;
    pvr = input;

    int position = 0;
    int oldPosition = -1;
    components.clear();
    separators.clear();
    while(position < input.count() && position != oldPosition)
    {
        oldPosition = position;

        match = portage->separator.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
        if(match.hasMatch())
        {
            separators.append(match.captured(1));
            position = match.capturedEnd();
        }
        else
        {
            separators.append(QString());
        }

        match = portage->digitVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
        if(match.hasMatch())
        {
            components.append(match.captured(1));
            position = match.capturedEnd();
        }
        else
        {
            match = portage->alphaVersion.match(input, position, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
            if(match.hasMatch())
            {
                components.append(match.captured(1));
                position = match.capturedEnd();
            }
            else
            {
                break; // this shouldn't happen
            }
        }
    }
}

QString VersionString::cut(int index)
{
    if(index >= 0 && index < components.count())
    {
        return components.at(index);
    }

    return QString();
}

QString VersionString::cutNoRevision(int index)
{
    if(index >= 0 && index < components.count())
    {
        if(index == 0)
        {
            return components.at(index);
        }

        if(components.at(index - 1) != "r")
        {
            if(components.at(index) != "r")
            {
                return components.at(index);
            }
        }
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

    return QString();
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
    QString s;
    int count = components.count();
    for(int i = 0; i < count; i++)
    {
        s = components.at(i);
        if(s == "r")
        {
            i++;
            if(i >= count)
            {
                break;
            }

            s = components.at(i);
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V6 >= %1").arg(escapeSql(s)));
            break;
        }

        if(i < 6)
        {
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V%1 >= %2").arg(i + 1).arg(escapeSql(s)));
        }
    }
}

void VersionString::lessThanEqualToSQL(QString& clauses)
{
    QString s;
    int count = components.count();
    for(int i = 0; i < count; i++)
    {
        s = components.at(i);
        if(s == "r")
        {
            i++;
            if(i >= count)
            {
                break;
            }

            s = components.at(i);
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V6 <= %1").arg(escapeSql(s)));
            break;
        }

        if(i < 6)
        {
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V%1 <= %2").arg(i + 1).arg(escapeSql(s)));
        }
    }
}

void  VersionString::greaterThanSQL(QString& clauses)
{
    QString s;
    int count = components.count();
    for(int i = 0; i < count; i++)
    {
        s = components.at(i);
        if(s == "r")
        {
            i++;
            if(i >= count)
            {
                break;
            }

            s = components.at(i);
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V6 > %1").arg(escapeSql(s)));
            break;
        }

        if(i < 6)
        {
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }

            if(i+1 < count && components.at(i+1) != "r")
            {
                clauses.append(QString("V%1 >= %2").arg(i + 1).arg(escapeSql(s)));
            }
            else
            {
                clauses.append(QString("V%1 > %2").arg(i + 1).arg(escapeSql(s)));
            }
        }
    }
}

void VersionString::lessThanSQL(QString& clauses)
{
    QString s;
    int count = components.count();
    for(int i = 0; i < count; i++)
    {
        s = components.at(i);
        if(s == "r")
        {
            i++;
            if(i >= count)
            {
                break;
            }

            s = components.at(i);
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }
            clauses.append(QString("V6 < %1").arg(escapeSql(s)));
            break;
        }

        if(i < 6)
        {
            if(clauses.isEmpty() == false)
            {
                clauses.append(" and ");
            }

            if(i+1 < count && components.at(i+1) != "r")
            {
                clauses.append(QString("V%1 <= %2").arg(i + 1).arg(escapeSql(s)));
            }
            else
            {
                clauses.append(QString("V%1 < %2").arg(i + 1).arg(escapeSql(s)));
            }
        }
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



/*
See https://mgorny.pl/articles/the-ultimate-guide-to-eapi-7.html#version-manipulation-and-comparison-commands
for ver_cut explanation.
*/


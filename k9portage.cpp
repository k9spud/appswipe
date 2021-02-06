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

#include "k9portage.h"

#include <QDir>
#include <QDebug>

K9Portage::K9Portage(QObject *parent) : QObject(parent)
{
    separator.setPattern("([^A-Za-z0-9]+)");
    digitVersion.setPattern("([0-9]+)");
    alphaVersion.setPattern("([A-Za-z]+)");

    stringAssignment.setPattern("(.+)\\s*=\\s*\"([^\"]*)\"");
    variableAssignment.setPattern("(.+)\\s*=\\s*([^\"]*)");
    verCutSingle.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)\\)");
    verCutRange.setPattern("\\$\\((ver_cut|get_version_component_range)\\s+([0-9]+)-([0-9]+)\\)");
    var_ref.setPattern("\\$\\{([A-z, 0-9, _]+)\\}");
}

void K9Portage::setRepoFolder(QString path)
{
    repoFolder = path;
    if(repoFolder.isEmpty())
    {
        repoFolder = "/var/db/repos/";
    }
    else if (repoFolder.endsWith('/') == false)
    {
        repoFolder.append('/');
    }

    QDir dir;
    dir.setPath(repoFolder);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    repos.clear();
    foreach(QString repoDir, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        repos.append(repoFolder + repoDir + "/");
    }

    categories.clear();
    foreach(QString repo, repos)
    {
        dir.setPath(repo);
        dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach(QString cat, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            if(categories.contains(cat) == false)
            {
                categories.append(cat);
            }
        }
    }
}

void K9Portage::setVersion(QString v)
{
    version.parse(v);
    vars["PVR"] = version.pvr;
    vars["PV"] = version.pv();
    vars["PR"] = version.pr();
    vars["P"] = QString("%1-%2").arg(vars["PN"]).arg(vars["PV"]);
    vars["PF"] = QString("%1-%2").arg(vars["PN"]).arg(version.pvr);
}

QVariant K9Portage::var(QString key)
{
    if(vars.contains(key))
    {
        return vars[key];
    }

    return QVariant(QVariant::String);
}

void K9Portage::parseVerCut(QString& value)
{
    bool matchFound;
    QRegularExpressionMatch match;

    do
    {
        matchFound = false;
        match = verCutRange.match(value);
        if(match.hasMatch())
        {
            matchFound = true;
            int c1 = match.captured(2).toInt();
            int c2 = match.captured(3).toInt();
            value = value.replace(match.capturedStart(), match.capturedLength(), QString("%1.%2").arg(version.cut(c1 - 1)).arg(version.cut(c2 - 1)));
        }

        match = verCutSingle.match(value);
        if(match.hasMatch())
        {
            matchFound = true;
            int c = match.captured(2).toInt();
            value = value.replace(match.capturedStart(), match.capturedLength(), QString("%1").arg(version.cut(c - 1)));
        }
    } while(matchFound);
}

void K9Portage::ebuildParser(QString data)
{
    QString key;
    QString value;
    QRegularExpressionMatch match;
    QRegularExpressionMatch vrmatch;

    QStringList ebuildStatements = data.split('\n');
    foreach(data, ebuildStatements)
    {
        data.remove('\n');
        data = data.trimmed();
        if(data.isEmpty() || data.startsWith('#'))
        {
            continue;
        }

        key.clear();
        value.clear();

        match = stringAssignment.match(data, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
        if(match.hasMatch() && match.lastCapturedIndex() >= 2)
        {
            key = match.captured(1).toUpper();
            value = match.captured(2);
        }
        else
        {
            match = variableAssignment.match(data, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
            if(match.hasMatch() && match.lastCapturedIndex() >= 2)
            {
                key = match.captured(1).toUpper();
                value = match.captured(2);
            }
        }

        if(key.isEmpty() == false)
        {
            parseVerCut(value);

            int index = 0;
            vrmatch = var_ref.match(value, index, QRegularExpression::PartialPreferFirstMatch, QRegularExpression::NoMatchOption);
            while(vrmatch.hasMatch())
            {
                QString replace = vrmatch.captured(0);
                QString var = vrmatch.captured(1);
                if(vars.contains(var))
                {
                    value = value.replace(replace, vars[var]);
                }
                else
                {
                    value = value.remove(replace);
                }
                vrmatch = var_ref.match(value, index, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
            }

            vars[key] = value;
        }
    }
}

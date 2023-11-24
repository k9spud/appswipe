// Copyright (c) 2021-2023, K9spud LLC.
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
#include "datastorage.h"
#include "globals.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QRegularExpressionMatch>

K9Portage::K9Portage(QObject *parent) : QObject(parent)
{
    separator.setPattern("([^A-Za-z0-9]+)");
    digitVersion.setPattern("([0-9]+)");
    alphaVersion.setPattern("([A-Za-z]+)");

    // see https://devmanual.gentoo.org/general-concepts/dependencies/
    // See Restrictions upon Names: https://projects.gentoo.org/pms/8/pms.html#x1-150003
    //                              https://dev.gentoo.org/~ulm/pms/head/pms.html#chapter-3
    dependKeywordsBasicRE.setPattern("([^~=><][^/]*)/([^:\\n\\s]+)\\s+(.+)$");
    dependKeywordsVersionRE.setPattern("(~|=|>=|<=|>|<)([^/]+)/([^:]+)-([0-9\\*][0-9\\-\\.A-z\\*]*)\\s+(.+)$");
    dependKeywordsSlotRE.setPattern("([^~=><][^/]*)/([^:\\n]+):([^:\\n\\s]+)\\s+(.+)$");
    dependKeywordsRepositoryRE.setPattern("(~|=|>=|<=|>|<)([^/]+)/(.+)-([0-9\\*][0-9\\-\\.A-z\\*]*):([^:]*):([^:\\n\\s]+)\\s+(.+)$");

    dependLinkRE.setPattern("(!!>=|!!<=|!!~|!!=|!!>|!!<|!>=|!<=|!~|!=|!>|!<|!!|>=|<=|~|=|>|<|!)([^/]+)/([^:]+)-([0-9\\*][^\\n]*)");
    dependLinkSlotRE.setPattern("(!!>=|!!<=|!!~|!!=|!!>|!!<|!>=|!<=|!~|!=|!>|!<|!!|>=|<=|~|=|>|<|!|)([^/]+)/([^:]+):([^\\n]+)");
    dependLinkAppRE.setPattern("(!!|!|)([^/]+)/([^\\[\\]\\n]+)(\\[[^\\n]+\\]|)");

#if defined(__x86_64__)
    arch = "amd64";
#elif defined(__i386__)
    arch = "x86";
#elif defined(__aarch64__)
    arch = "arm64";
#elif defined(__arm__)
    arch = "arm";
#elif defined(__mips__)
    arch = "mips";
#elif defined(__ppc64__)
    arch = "ppc64";
#elif defined(__powerpc__)
    arch = "ppc";
#elif defined(__sparc__)
    arch = "sparc";
#elif defined(__m68k__)
    arch = "m68k";
#elif defined(__alpha__)
    arch = "alpha";
#elif defined(__hppa__)
    arch = "hppa";
#elif defined(__ia64__)
    arch = "ia64";
#elif defined(__riscv__)
    arch = "riscv";
#elif defined(__s390__)
    arch = "s390";
#endif

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

QVariant K9Portage::var(QString key)
{
    if(vars.contains(key))
    {
        return vars[key];
    }

    return QVariant(QVariant::String);
}

QString K9Portage::linkDependency(QString input, QString& category, QString& package)
{
    QRegularExpressionMatch match = dependLinkRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString version = match.captured(4);
        filter.replace("<", "&lt;");
        filter.replace(">", "&gt;");
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>-%4").arg(filter, category, package, version);
        }
    }

    match = dependLinkSlotRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString slot = match.captured(4);
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>:%4").arg(filter, category, package, slot);
        }
    }

    match = dependLinkAppRE.match(input);
    if(match.hasMatch())
    {
        QString filter = match.captured(1);
        category = match.captured(2);
        package = match.captured(3);
        QString uses = match.captured(4);
        filter.replace("<", "&lt;");
        filter.replace(">", "&gt;");
        if(category.contains("*") == false && package.contains("*") == false)
        {
            return QString("%1<A HREF=\"app:%2/%3\" <LINKCOLOR>>%2/%3</A>%4").arg(filter, category, package, uses);
        }
    }

    input.replace("<", "&lt;");
    input.replace(">", "&gt;");
    return input;
}

void K9Portage::md5cacheReader(QString fileName)
{
    QFile input(fileName);
    if(!input.exists())
    {
        output << "md5-cache " << input.fileName() << " does not exist." << Qt::endl;
        return;
    }

    if(!input.open(QIODevice::ReadOnly))
    {
        output << "md5-cache " << input.fileName() << " could not be opened for reading." << Qt::endl;
        return;
    }

    QString s;
    QString key;
    QString value;
    QRegularExpressionMatch match;
    QTextStream in(&input);
    int i;
    while(in.atEnd() == false)
    {
        s = in.readLine();
        s = s.trimmed();
        if(s.isEmpty() || s.startsWith('#'))
        {
            continue;
        }

        i = s.indexOf('=');
        if(i == -1)
        {
            continue;
        }
        key = s.left(i);
        value = s.mid(i + 1);
        vars[key] = value;
    }
}

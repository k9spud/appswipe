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

#include "main.h"
#include "globals.h"
#include "k9portage.h"
#include "datastorage.h"

#include <signal.h>
#include <QApplication>
#include <QProcessEnvironment>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QUuid>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>
#include <QImageReader>
#include <QTextStream>
#include <QHash>

bool isWorld = false;
QHash<QString, QString> iconMap;    // category type, icon resource file name
int viewWidth = 0, viewHeight = 0;

void viewApp(const QUrl& url);
QString appVersion(QString app);
QString appNoVersion(QString app);
void removeDuplicateDeps(QStringList& target, QStringList& source);
int outerDepMatch(QStringList& target, int& targetIndex, QStringList& source, int sourceIndex);
void skipNode(QStringList& nodes, int& index);
int depMatch(QStringList& target, int& targetIndex, QStringList& source, int sourceIndex);
QString findAppIcon(bool& hasIcon, QString category, QString package, QString version);
QString printDependencies(QStringList dependencies, QSqlQuery& query, bool flagMissing);

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("K9spud LLC");
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QApplication a(argc, argv);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if(env.contains("RET_CODE"))
    {
        // abort if emerge process failed to actually do anything
        int retCode = env.value("RET_CODE").toInt();
        if(retCode != 0)
        {
            return retCode;
        }
    }

    if(argc <= 1)
    {
        return -1;
    }

    portage = new K9Portage();
    portage->setRepoFolder("/var/db/repos/");

    ds = new DataStorage();
    ds->openDatabase();

    iconMap["app"] = ":/img/app.svg";
    iconMap["dev"] = ":/img/dev.svg";
    iconMap["games"] = ":/img/games.svg";
    iconMap["gnustep"] = ":/img/gnustep.svg";
    iconMap["gnome"] = ":/img/gnome.svg";
    iconMap["gui"] = ":/img/x11.svg";
    iconMap["kde"] = ":/img/kde.svg";
    iconMap["mail"] = ":img/mail.svg";
    iconMap["media"] = ":/img/media.svg";
    iconMap["net"] = ":/img/net.svg";
    iconMap["sci"] = ":/img/sci.svg";
    iconMap["sys"] = ":/img/sys.svg";
    iconMap["www"] = ":/img/www.svg";
    iconMap["x11"] = ":/img/x11.svg";
    iconMap["xfce"] = ":/img/xfce.svg";

    QString urlText = argv[1];
    for (int i = 1; i < argc; ++i)
    {
        if(qstrcmp(argv[i], "-width") == 0)
        {
            i++;
            if(i < argc)
            {
                viewWidth = atoi(argv[i]);
            }
            continue;
        }

        if(qstrcmp(argv[i], "-height") == 0)
        {
            i++;
            if(i < argc)
            {
                viewHeight = atoi(argv[i]);
            }
            continue;
        }

        urlText = argv[i];
    }

    QUrl url = urlText;
    viewApp(url);

    return 0;
}

void viewApp(const QUrl& url)
{
    QSqlDatabase db;
    if(QSqlDatabase::contains("GuiThread") == false)
    {
        db = QSqlDatabase::addDatabase("QSQLITE", "GuiThread");
    }
    else
    {
        db = QSqlDatabase::database("GuiThread");
        if(db.isValid())
        {
            db.close();
        }
    }

    db.setDatabaseName(ds->storageFolder + ds->databaseFileName);
    db.open();
    QSqlQuery query(db);
    query.prepare(R"EOF(
select
    r.REPO, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.OBSOLETED, p.SLOT, p.HOMEPAGE, p.LICENSE,
    p.KEYWORDS, p.IUSE, c.CATEGORY, p.PACKAGE, p.MASKED, p.DOWNLOADSIZE, p.PACKAGEID, p.SUBSLOT
from PACKAGE p
inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID
inner join REPO r on r.REPOID = p.REPOID
where c.CATEGORY=? and p.PACKAGE=?
order by p.PACKAGE, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc, p.V7 desc, p.V8 desc, p.V9 desc, p.V10 desc
)EOF");

    QString search = url.path(QUrl::FullyDecoded);
    QStringList x = search.split('/');
    QString category = x.first();
    QString package = appNoVersion(x.last());
    QString version = appVersion(x.last());
    int packageId = 0;
    query.bindValue(0, category);
    query.bindValue(1, package);

    bool foundApp = query.exec() && query.first();
    if(foundApp == false)
    {
        // try again, without stripping what we thought was a version number last time...
        package = x.last();
        version.clear();
        query.bindValue(0, category);
        query.bindValue(1, package);
        foundApp = query.exec() && query.first();
    }

    bool installed;
    bool obsoleted;
    int masked;
    bool hasIcon = false;
    QString repo;
    QString slot;
    QString subslot;
    QString description;
    QString homePage;
    QString license;
    QString keywords;
    int downloadSize;
    QString ebuild;
    QString appicon;
    QString action;
    QString installedSize;
    QString iuse;
    QString useFlags;
    QString cFlags;
    QString cxxFlags;
    QString installDepend;  // DEPEND - dependencies for CHOST, i.e. packages that need to be found on built system, e.g. libraries and headers.
    QString buildDepend;    // BDEPEND - dependencies applicable to CBUILD, i.e. programs that need to be executed during the build, e.g. virtual/pkgconfig.
    QString runDepend;      // RDEPEND - dependencies which are required at runtime, such as libraries (when dynamically linked), any data packages and (for interpreted languages) the relevant interpreter. When installing from a binary package, only RDEPEND will be checked.
    QString postDepend;     // PDEPEND - runtime dependencies that do not strictly require being satisfied immediately. They can be merged after the package.
    QString lastBuilt;
    QStringList runtimeDeps;
    QStringList optionalDeps;
    QStringList installtimeDeps;
    int firstUnmasked = -1;
    int firstMaskedTesting = -1;
    int i;
    QString s;

    QString versionSize;

    QFile input;

    output << "<HTML>\n";
    output << QString("<HEAD><TITLE>%1</TITLE></HEAD>\n<BODY>").arg(package);
    isWorld = false;
    if(foundApp)
    {
        while(1)
        {
            installed = query.value(4).toBool();
            if(version.isEmpty() == false)
            {
                if(version == query.value(2).toString())
                {
                    break;
                }
            }
            else if(installed)
            {
                break;
            }

            if(firstUnmasked == -1)
            {
                keywords = query.value(9).toString();
                masked = query.value(13).toInt();
                if(masked == 0)
                {
                    firstUnmasked = query.at();
                }
                else if(masked == 2 && firstMaskedTesting == -1) // testing, but still masked
                {
                    firstMaskedTesting = query.at();
                }
            }

            if(query.next() == false)
            {
                if(firstUnmasked != -1)
                {
                    query.seek(firstUnmasked);
                }
                else
                {
                    if(firstMaskedTesting != -1)
                    {
                        firstUnmasked = firstMaskedTesting;
                        query.seek(firstUnmasked);
                    }
                    else
                    {
                        query.first();
                    }
                }
                break;
            }
        }

        // determine icon file
        category = query.value(11).toString();
        package = query.value(12).toString();
        version = query.value(2).toString();
        iuse = query.value(10).toString();
        packageId = query.value(15).toInt();

        appicon = findAppIcon(hasIcon, category, package, version);

        description = query.value(3).toString();
        homePage = query.value(7).toString();
        license = query.value(8).toString();
        keywords = query.value(9).toString();
        category = query.value(11).toString();
        package = query.value(12).toString();
        installed = query.value(4).toBool();
        repo = query.value(0).toString();

        if(installed)
        {
            s = QString("/var/db/pkg/%1/%2-%3/USE").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                useFlags = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/CFLAGS").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                cFlags = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/CXXFLAGS").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                cxxFlags = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/DEPEND").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                installDepend = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/BDEPEND").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                buildDepend = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/RDEPEND").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                runDepend = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/PDEPEND").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                postDepend = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/BUILD_TIME").arg(category, package, version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                lastBuilt = input.readAll();
                input.close();
            }
        }
        else
        {
            s = QString("/var/db/repos/%1/metadata/md5-cache/%2/%3-%4").arg(repo, category, package, version);
            if(QFile::exists(s))
            {
                portage->vars.clear();
                portage->md5cacheReader(s);
                installDepend = portage->var("DEPEND").toString();
                buildDepend = portage->var("BDEPEND").toString();
                runDepend = portage->var("RDEPEND").toString();
                postDepend = portage->var("PDEPEND").toString();
            }
        }

        s = QString("/var/lib/portage/world");
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly))
        {
            QString worldSet = input.readAll();
            input.close();
            if(worldSet.contains(QString("%1/%2\n").arg(category, package)) ||
               worldSet.contains(QString("%1/%2:%3\n").arg(category, package, slot)) ||
               worldSet.contains(QString("%1/%2:%3/%4\n").arg(category, package, slot, subslot)) ||
               worldSet.contains(QString("%1/%2::%3\n").arg(category, package, repo)) ||
               worldSet.contains(QString("%1/%2-%3\n").arg(category, package, version)))
            {
                isWorld = true;
            }
        }

        if(homePage.contains(' '))
        {
            QStringList pages = homePage.split(' ');
            homePage.clear();
            foreach(QString s, pages)
            {
                homePage.append(QString("<A HREF=\"%1\">%1</A> ").arg(s));
            }
        }
        else
        {
            homePage = QString("<A HREF=\"%1\">%1</A>").arg(homePage);
        }

        if(hasIcon == false)
        {
            QString cat = category.left(category.indexOf('-'));
            if(iconMap.contains(cat))
            {
                error << "icon " << iconMap[cat] << Qt::endl;
            }
            else
            {
                error << "icon :/img/page.svg" << Qt::endl;
            }
        }

        if(appicon.isEmpty())
        {
            output << QString(R"EOF(<P><B><A HREF="clip:%1">%1</A></B><BR>%2</P><P>%3</P>)EOF").arg(search, description, homePage);
        }
        else
        {
            int w = 0, h = 0;
            if(viewWidth != 0 && appicon.contains("128x128") == false)
            {
                QImage image;
                QImageReader reader;
                reader.setDecideFormatFromContent(true);
                reader.setFileName(appicon);
                if(reader.read(&image) == false || image.isNull())
                {
                    qDebug() << appicon << reader.errorString();
                }
                else
                {
                    w = image.width();
                    h = image.height();
                    if(w > static_cast<float>(viewWidth) * 0.50)
                    {
                        h = static_cast<float>(h) * ((static_cast<float>(viewWidth * 0.50)) / static_cast<float>(w));
                        w = static_cast<float>(viewWidth) * 0.50;
                    }
                }
            }

            output << "<TABLE><TR><TD>";
            if(h != 0 && w != 0)
            {
                output << QString("<IMG SRC=\"%1\" WIDTH=%2 HEIGHT=%3>").arg(appicon).arg(w).arg(h);
            }
            else
            {
                output << QString("<IMG SRC=\"%1\">").arg(appicon);
            }
            output << QString("</TD><TD><P><BR><B>&nbsp;<A HREF=\"clip:%1/%2\">%1/%2</A></B></P><P>&nbsp;%3</P><P>&nbsp;%4</P></TD></TR></TABLE>").arg(category, package, description, homePage); // << Qt::endl;
        }

        output << ("<P><TABLE BORDER=0 CLASS=\"normal\">");
        output << ("<TR><TD></TD><TD><B>Repository&nbsp;&nbsp;</B></TD><TD><B>Slot&nbsp;&nbsp;</B></TD><TD><B>Version&nbsp;&nbsp;</B></TD><TD><B>Status</B>");
        if(isWorld)
        {
            output << (" (@world member)");
        }
        output << ("</TD></TR>\n");
        output << ("<TR><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD></TR>");

        int rowId;
        query.first();
        do
        {
            repo = query.value(0).toString();
            version = query.value(2).toString();
            bool rowInstalled = query.value(4).toBool();
            obsoleted = query.value(5).toBool();
            masked = query.value(13).toInt();
            rowId = query.value(15).toInt();
            slot = query.value(6).toString();
            subslot = query.value(16).toString();
            downloadSize = query.value(14).toInt();
            if(downloadSize > 0)
            {
                installedSize = QString(": <A HREF=\"files:%1/%2-%3\">%4</A>").arg(category, package, version, fileSize(downloadSize));
            }
            else
            {
                installedSize.clear();
            }

            if(rowInstalled)
            {
                ebuild = QString("/var/db/pkg/%1/%2-%3/%2-%3.ebuild").arg(category, package, version);
            }
            else
            {
                ebuild = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(repo, category, package, version);
            }

            s = query.value(9).toString(); // PACKAGE.KEYWORD
#define TESTING (1<<1)
#define UNSUPPORTED (1<<2)
#define BROKEN (1<<3)
            if(masked & 1)
            {
                s = "masked";
            }
            else if(masked & TESTING)
            {
                s = "testing";
            }
            else if(masked & BROKEN)
            {
                s = "broken";
            }
            else if(masked & UNSUPPORTED)
            {
                s = "unsupported";
            }
            else if(masked == 0)
            {
                if(s.contains(QString("-%1").arg(portage->arch)) || (s.contains("-*") && s.contains(portage->arch) == false))
                {
                    s = "broken";
                    if(!rowInstalled)
                    {
                        s.append(", unmasked");
                    }
                }
                else if(s.contains(portage->arch) == false)
                {
                    s = "unsupported";
                    if(!rowInstalled)
                    {
                        s.append(", unmasked");
                    }
                }
                else
                {
                    i = s.indexOf(portage->arch);
                    if(i == 0 || s.at(i - 1) == ' ')
                    {
                        s = "stable";
                    }
                    else if(i > 0 && s.at(i-1) == '~')
                    {
                        s = "testing";
                        if(!rowInstalled)
                        {
                            s.append(", unmasked");
                        }
                    }
                }
            }
            else
            {
                s = "unknown";
            }

            s = s + (obsoleted ? " (obsolete" + installedSize + ")" :
                     rowInstalled ? " (installed" + installedSize + ")" : "");
            if(rowInstalled)
            {
                action = "uninstall";
            }
            else
            {
                if(masked)
                {
                    action = "unmask";
                }
                else
                {
                    action = "install";
                }
            }

            versionSize = QString("<A HREF=\"%1:%2/%3-%4\">%4</A>").arg(action, category, package, version);
            output << ("<TR>");
            if(rowId == packageId)
            {
                output << ("<TD>&#9656;</TD>");
            }
            else
            {
                output << ("<TD></TD>");
            }
            output << (QString("<TD><A HREF=\"file://%2\">%1</A>&nbsp;&nbsp;</TD>").arg(repo, ebuild));

            if(subslot.isEmpty())
            {
                output << (QString("<TD>%1&nbsp;&nbsp;</TD>").arg(slot));
            }
            else
            {
                output << (QString("<TD>%1/%2&nbsp;&nbsp;</TD>").arg(slot, subslot));
            }

            output << (QString("<TD>%1&nbsp;&nbsp;</TD>").arg(versionSize));
            output << (QString("<TD>%1</TD>").arg(s));
            output << ("</TR>\n");
        } while(query.next());
        output << "</TABLE></P>";
        query.clear();

        if(license.isEmpty() == false)
        {
            output << "<P><B>License: </B>" << Qt::flush << QString("%1</P>").arg(license);
        }

        QStringList iuseList = iuse.remove("\n").split(' ', Qt::SkipEmptyParts);
        QStringList useList;
        int useListCount = 0;
        if(useFlags.isEmpty() == false)
        {
            QString useHtml;
            QString flag;
            QString prefix;

            useList = useFlags.remove("\n").split(' ', Qt::SkipEmptyParts);
            useListCount = useList.count();
            for(i = 0; i < useListCount; i++)
            {
                s = useList.at(i);
                if(s.startsWith('+') || s.startsWith('-'))
                {
                    prefix = s.at(0);
                    flag = s.mid(1);
                }
                else
                {
                    prefix.clear();
                    flag = s;
                }


                if(iuseList.contains(flag) == false && iuseList.contains("+" + flag) == false && iuseList.contains("-" + flag) == false)
                {
                    continue;
                }

                useHtml.append(QString("%2<A HREF=\"use:%2%1?%3/%4\">%1</A> ").arg(flag, prefix, category, package));
            }

            if(useHtml.isEmpty() == false)
            {
                output << "<P><B>Applied Flags:</B> " << Qt::flush << QString("%1</P>\n").arg(useHtml);
            }
        }

        if(iuse.isEmpty() == false)
        {
            iuseList.removeDuplicates();
            for(i = 0; i < useListCount; i++)
            {
                s = useList.at(i);
                if(iuseList.contains(s))
                {
                    iuseList.removeAll(s);
                }

                s.prepend('+');
                if(iuseList.contains(s))
                {
                    iuseList.removeAll(s);
                }

                s.prepend('-');
                if(iuseList.contains(s))
                {
                    iuseList.removeAll(s);
                }
            }

            const int iuseListCount = iuseList.count();
            if(iuseListCount)
            {
                output << "<P><B>Unused Flags:</B> " << Qt::flush;
                QString s;
                QString flag;
                QString prefix;
                for(i = 0; i < iuseListCount; i++)
                {
                    s = iuseList.at(i);
                    if(s.startsWith('+') || s.startsWith('-'))
                    {
                        prefix = s.at(0);
                        flag = s.mid(1);
                    }
                    else
                    {
                        prefix.clear();
                        flag = s;
                    }

                    output << QString("%2<A HREF=\"use:%2%1?%3/%4\">%1</A> ").arg(flag, prefix, category, package);
                }
                output << ("</P>\n");
            }
        }

        if(lastBuilt.isEmpty() == false)
        {
            QDateTime lastBuiltDT;
            lastBuiltDT.setSecsSinceEpoch(lastBuilt.toInt());
            output << "<P><B>Last built:</B> " << Qt::flush << QString("%1</P>").arg(lastBuiltDT.toString("dddd MMMM d, yyyy h:mm AP"));
        }

        if(cFlags.isEmpty() == false)
        {
            output << "<P><B>CFLAGS:</B> " << Qt::flush << QString("%1</P>").arg(cFlags);
        }

        if(cxxFlags.isEmpty() == false && cxxFlags != cFlags)
        {
            output << QString("<P><B>CXXFLAGS:</B> %1</P>").arg(cxxFlags);
        }

        if(keywords.isEmpty() == false)
        {
            output << "<P><B>Keywords:</B> " << Qt::flush << QString("%1</P>").arg(keywords);
        }

        if(runDepend.isEmpty() == false)
        {
            runtimeDeps = runDepend.remove("\n").split(' ');
            if(runtimeDeps.count())
            {
                if(installDepend == runDepend)
                {
                    output << "<P><B>Dependencies" << Qt::flush << ":</B></P><P>";
                }
                else
                {
                    output << "<P><B>Dependencies for run-time use" << Qt::flush << ":</B></P><P>";
                }

                output << printDependencies(runtimeDeps, query, (installed == 0));
                output << "</P>";
            }
        }

        if(postDepend.isEmpty() == false)
        {
            optionalDeps = postDepend.remove("\n").split(' ');
            if(optionalDeps.count())
            {
                output << "<P><B>Dependencies not strictly required at run-time" << Qt::flush << ":</B></P><P>";
                output << printDependencies(optionalDeps, query, (installed == 0));
                output << "</P>";
            }
        }

        if(installDepend.isEmpty() == false && runDepend != installDepend)
        {
            installtimeDeps = installDepend.remove("\n").split(' ');
            removeDuplicateDeps(installtimeDeps, runtimeDeps);
            removeDuplicateDeps(installtimeDeps, optionalDeps);
            if(installtimeDeps.count())
            {
                output << "<P><B>Dependencies for installing package" << Qt::flush << ":</B></P><P>";
                output << printDependencies(installtimeDeps, query, (installed == 0));
                output << "</P>";
            }
        }

        if(buildDepend.isEmpty() == false && runDepend != buildDepend)
        {
            QStringList deps = buildDepend.remove("\n").split(' ');
            removeDuplicateDeps(deps, runtimeDeps);
            removeDuplicateDeps(deps, optionalDeps);
            removeDuplicateDeps(deps, installtimeDeps);
            if(deps.count())
            {
                output << "<P><B>Dependencies for building from source" << Qt::flush << ":</B></P><P>";
                output << printDependencies(deps, query, (installed == 0));
                output << "</P>";
            }
        }
    }

    output << ("<P>&nbsp;<BR></P>\n");
    output << ("</BODY>\n<HTML>\n") << Qt::endl;
    error << "title " << package << Qt::endl;
    error << "isWorld " << (isWorld ? "1" : "0") << Qt::endl;
}

int depMatch(QStringList& target, int& targetIndex, QStringList& source, int sourceIndex)
{
    if(targetIndex < 0 || sourceIndex < 0)
    {
        return -1;
    }

    int parens = 0;
    int i = targetIndex;
    int j = sourceIndex;
    bool foundEnd = false;
    QString s;
    while(i < target.count() && j < source.count() && foundEnd == false)
    {
        s = target.at(i).trimmed();
        if(s == "(")
        {
            parens++;
        }
        else if(s == ")")
        {
            parens--;
            if(parens == 0)
            {
                foundEnd = true;
            }
        }

        if(j >= source.count())
        {
            return -1;
        }

        if(s != source.at(j))
        {
            return -1;
        }

        i++;
        j++;
    }

    targetIndex = i;
    return i;
}

void skipNode(QStringList& nodes, int& index)
{
    int parens = 0;
    QString s;
    bool foundEnd = false;
    while(index < nodes.count() && foundEnd == false)
    {
        s = nodes.at(index).trimmed();
        if(s == "(")
        {
            parens++;
        }
        else if(s == ")")
        {
            parens--;
            if(parens == 0)
            {
                foundEnd = true;
            }
        }
        index++;
    }
}

int outerDepMatch(QStringList& target, int& targetIndex, QStringList& source, int sourceIndex)
{
    QString s = target.at(targetIndex);
    int j = source.indexOf(s, sourceIndex);
    if(j < 0)
    {
        skipNode(target, targetIndex);
        return -1;
    }

    int result = depMatch(target, targetIndex, source, j);
    if(result >= 0)
    {
        return result;
    }

    skipNode(source, j);
    return outerDepMatch(target, targetIndex, source, j);
}

void removeDuplicateDeps(QStringList& target, QStringList& source)
{
    int i = 0;
    QString s;
    int j;
    int endMatch;
    int removeStart;
    int sourceSearch;
    while(i < target.count())
    {
        s = target.at(i).trimmed();
        if(s.endsWith('?') || s == "||")
        {
            removeStart = i;
            sourceSearch = 0;
            j = source.indexOf(s, sourceSearch);
            endMatch = outerDepMatch(target, i, source, j);
            if(endMatch >= 0)
            {
                j = removeStart;
                i = removeStart;
                while(i < target.count() && j < endMatch)
                {
                    target.removeAt(i);
                    j++;
                }
            }

            continue;
        }
        else if(source.contains(s))
        {
            // don't show target dependencies that are already listed in the source dependencies list
            target.removeAt(i);
            continue;
        }
        i++;
    }
}

QString appNoVersion(QString app)
{
    int versionIndex = app.lastIndexOf('-');
    if(versionIndex < app.count() && app.at(versionIndex + 1) == 'r')
    {
        versionIndex = app.lastIndexOf('-', versionIndex - 1);
    }

    QChar number = app.at(versionIndex + 1);
    if(number >= '0' && number <= '9')
    {
        // looks like a valid version number
        return app.left(versionIndex);
    }

    // maybe this string didn't have a version number at all?
    return app;
}

QString appVersion(QString app)
{
    int versionIndex = app.lastIndexOf('-');
    if(versionIndex < app.count() && app.at(versionIndex + 1) == 'r')
    {
        versionIndex = app.lastIndexOf('-', versionIndex - 1);
    }

    QChar number = app.at(versionIndex + 1);
    if(number >= '0' && number <= '9')
    {
        // looks like a valid version number
        return app.mid(versionIndex + 1);
    }

    // maybe this string didn't have a version number at all?
    return "";
}

QString findAppIcon(bool& hasIcon, QString category, QString package, QString version)
{
    QFile input;
    QStringList bigIcons;
    QStringList smallIcons;
    QStringList desktopFiles;
    QString s;
    int i;
    QString appicon = QString("/var/db/pkg/%1/%2-%3/CONTENTS").arg(category, package, version);
    input.setFileName(appicon);
    if(!input.open(QIODevice::ReadOnly))
    {
        return "";
    }

    s = input.readAll();
    input.close();
    QStringList lines = s.split('\n', Qt::SkipEmptyParts);
    const int lineCount = lines.count();
    for(i = 0; i < lineCount; i++)
    {
        s = lines.at(i);
        s.remove('\n');
        s = s.trimmed();
        if(s.startsWith('#') || s.isEmpty())
        {
            continue;
        }

        if(s.contains(".desktop "))
        {
            QStringList fields = s.split(' ');
            if(fields.count() >= 4 && fields.at(0) == "obj")
            {
                desktopFiles.append(fields.at(1));
            }
        }
        else if(s.contains("/usr/share/icons/") || s.contains("/usr/share/pixmaps/"))
        {
            QStringList fields = s.split(' ');
            if(fields.count() >= 4 && fields.at(0) == "obj")
            {
                if((s.contains("128x128") || s.endsWith(".svg")) && bigIcons.count() && (bigIcons.first().endsWith(".svg") == false))
                {
                    bigIcons.prepend(fields.at(1));
                }
                else
                {
                    bigIcons.append(fields.at(1));
                }

                if(s.contains("32x32") || s.endsWith(".svg"))
                {
                    smallIcons.prepend(fields.at(1));
                }
                else
                {
                    smallIcons.append(fields.at(1));
                }
            }
        }
    }

    if(bigIcons.count())
    {
        appicon = bigIcons.first();
    }
    else
    {
        appicon.clear();
    }

    if(smallIcons.count())
    {
        error << "icon " << smallIcons.first() << Qt::endl;

        hasIcon = true;
    }

    return appicon;
}

QString printDependencies(QStringList dependencies, QSqlQuery& query, bool flagMissing)
{
    QString s;
    QString category;
    QString package;
    QStringList missingAtoms;
    QStringList upgradableAtoms;
    if(true || flagMissing)
    {
        for(int i = 0; i < dependencies.count(); i++)
        {
            s = dependencies.at(i);
            category.clear();
            package.clear();
            s = portage->linkDependency(s, category, package);
            if(category.count() && package.count())
            {
                s = category;
                s.append('/');
                s.append(package);
                if(missingAtoms.contains(s) == false)
                {
                    missingAtoms.append(s);
                }
            }
        }

        s = R"EOF(
    select c.CATEGORY, p.PACKAGE, p.VERSION from PACKAGE p
    inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID
    where p.INSTALLED != 0 and c.CATEGORY || '/' || p.PACKAGE in (
    )EOF";

        if(missingAtoms.count() > 0)
        {
            QString q = ",?";

            s.append('?');
            s.append(q.repeated(missingAtoms.count() - 1));
            s.append(")");

            query.prepare(s);
            foreach(s, missingAtoms)
            {
                query.addBindValue(s);
            }

            if(query.exec() && query.first())
            {
                do
                {
                    s = QString("%1/%2").arg(query.value(0).toString(), query.value(1).toString());
                    missingAtoms.removeAll(s);
                } while(query.next());
            }
        }
    }

    // ============================================================================================
    // flag upgradable dependencies
    // ============================================================================================
    s =
        R"EOF(
            select c.CATEGORY, p.PACKAGE
            from PACKAGE p
            inner join CATEGORY c on c.CATEGORYID=p.CATEGORYID
            inner join PACKAGE p2 on p2.PACKAGE=p.PACKAGE and p2.CATEGORYID=p.CATEGORYID and
                p2.SLOT is p.SLOT and p2.PACKAGEID != p.PACKAGEID and p2.INSTALLED=0 and p2.MASKED=0 and
                p2.VERSION != '9999' and p2.VERSION != '99999' and p2.VERSION != '999999' and p2.VERSION != '9999999' and p2.VERSION != '99999999' and p2.VERSION != '999999999' and
            (
                (p2.V1 > p.V1) or
                (p2.V1 is p.V1 and p2.V2 > p.V2) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 > p.V3) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 > p.V4) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 > p.V5) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and p2.V6 > p.V6) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and p2.V6 is p.V6 and p2.V7 > p.V7) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and p2.V6 is p.V6 and p2.V7 is p.V7 and p2.V8 > p.V8) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and p2.V6 is p.V6 and p2.V7 is p.V7 and p2.V8 is p.V8 and p2.V9 > p.V9) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and p2.V6 is p.V6 and p2.V7 is p.V7 and p2.V8 is p.V8 and p2.V9 is p.V9 and p2.V10 > p.V10)
            )
            where p.INSTALLED != 0 and
            (
                (p.STATUS is null or p.STATUS=0) or
                (p.STATUS=1 and p2.STATUS>=1) or
                (p.STATUS=2 and p2.STATUS=2)
            )
            limit 10000
            )EOF";

    query.prepare(s);
    if(query.exec() && query.first())
    {
        do
        {
            s = QString("%1/%2").arg(query.value(0).toString(), query.value(1).toString());
            upgradableAtoms.append(s);
        } while(query.next());
    }

    QString html;
    bool newline = false;
    int parens = 0;
    QString atom;
    for(int i = 0; i < dependencies.count(); i++)
    {
        s = dependencies.at(i);
        s = portage->linkDependency(s.replace(',', ' '), category, package);

        if(newline)
        {
            html.append("<BR>\n");
        }

        if(s == "(")
        {
            for(int j = 0; j <= parens; j++)
            {
                html.append("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
            }
            parens++;
        }
        else
        {
            if(s == ")")
            {
                parens--;
            }

            for(int j = 0; j <= parens; j++)
            {
                html.append("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
            }
        }

        atom = QString("%1/%2").arg(category, package);
        if(missingAtoms.contains(atom))
        {
            html.append(s.replace("<LINKCOLOR>", "STYLE=\"color: #fb9f9f\""));
        }
        else if(upgradableAtoms.contains(atom))
        {
            html.append(s.replace("<LINKCOLOR>", "STYLE=\"color: #fbff9d\""));
        }
        else
        {
            html.append(s.replace("<LINKCOLOR>", ""));
        }
        newline = true;
    }

    return html;
}

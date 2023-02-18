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

#include "browserview.h"
#include "compositeview.h"
#include "datastorage.h"
#include "k9portage.h"
#include "globals.h"
#include "k9shell.h"
#include "browserwindow.h"
#include "main.h"
#include "versionstring.h"
#include "tabwidget.h"
#include "history.h"

#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDebug>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextLength>
#include <QAbstractTextDocumentLayout>
#include <QBrush>
#include <QTextCursor>
#include <QResizeEvent>
#include <QDateTime>
#include <QClipboard>
#include <QPushButton>
#include <QStringList>
#include <QInputDialog>

#define START_SWIPE_THRESHOLD 20
#define RELOAD_THRESHOLD 250
#define LONGPRESS_MSECS_THRESHOLD 600

BrowserView::BrowserView(QWidget *parent) : QTextEdit(parent)
{
    composite = qobject_cast<CompositeView*>(parent);
    process = nullptr;
    document()->setDefaultStyleSheet(R"EOF(
                                     a { color: rgb(181, 229, 229); }

                                     table.normal
                                     {
                                         color: white;
                                         border-style: none;
                                         background-color: #101A20;
                                     }
)EOF");

    setReadOnly(true);
    setTabChangesFocus(false);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
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

    scrollGrabbed = false;
    swiping = false;
    connect(&animationTimer, SIGNAL(timeout()), this, SLOT(swipeUpdate()));
}

QPoint BrowserView::scrollPosition()
{
    QPoint value;
    QScrollBar* h = horizontalScrollBar();
    QScrollBar* v = verticalScrollBar();
    if(h != nullptr)
    {
        value.setX(h->value());
    }

    if(v != nullptr)
    {
        value.setY(v->value());
    }
    return value;
}

void BrowserView::setScrollPosition(const QPoint& pos)
{
    QScrollBar* h = horizontalScrollBar();
    QScrollBar* v = verticalScrollBar();

    if(h != nullptr)
    {
        h->setValue(pos.x());
    }

    if(v != nullptr)
    {
        v->setValue(pos.y());
    }
}

bool BrowserView::find(const QString& text, QTextDocument::FindFlags options)
{
    bool found = QTextEdit::find(text, options);
    if(found && (options & QTextDocument::FindBackward) == 0)
    {
        // When searching forward, we don't want the highlighted line to be on the very bottom of the
        // window, because that can be occluded by the status bar. So, just to be safe, lets
        // move down a couple lines and then move back -- this will scroll the view downwards enough
        // to avoid the status bar.

        QTextCursor cursor = textCursor();
        QTextCursor orig = cursor;
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
        setTextCursor(cursor);
        setTextCursor(orig);
    }

    return found;
}

void BrowserView::navigateTo(QString text, bool changeHistory, bool feelingLucky)
{
    if(text.isEmpty())
    {
        about();
    }
    else
    {
        if(text.startsWith("clip:"))
        {
           QClipboard* clip = qApp->clipboard();
           clip->setText(text.mid(5));
           composite->setStatus(QString("'%1' copied to the clipboard.").arg(text.mid(5)));
           return;
        }

        if(text.startsWith("update:fetch"))
        {
            viewUpdates("fetch");
            return;
        }

        QPoint pos;
        if(changeHistory &&
           !text.startsWith("install:") &&
           !text.startsWith("uninstall:") &&
           !text.startsWith("unmask:"))
        {
            pos = scrollPosition();
        }

        if(text.contains(':'))
        {
            setUrl(text);
        }
        else
        {
            currentUrl = text;
            searchApps(text, feelingLucky);
        }

        if(changeHistory &&
           !text.startsWith("install:") &&
           !text.startsWith("uninstall:") &&
           !text.startsWith("unmask:"))
        {
            History::State state;
            state.target = currentUrl;
            state.title = documentTitle();
            state.pos = pos;
            emit appendHistory(state);
            emit urlChanged(currentUrl);
        }

        composite->setStatus("");
        oldLink.clear();
        viewport()->unsetCursor();

        if(text.startsWith("files:") == false) // can't emit loadFinished for files: because that one uses an external process.
        {
            emit loadFinished();
        }
    }
}

void BrowserView::jumpTo(const History::State& s)
{
    navigateTo(s.target, false, false);
    emit urlChanged(s.target);
}

void BrowserView::setUrl(const QUrl& url)
{
    QString scheme = url.scheme();

    if(scheme == "https" || scheme == "http")
    {
        shell->externalBrowser(url.toString());
    }
    else if(scheme == "install")
    {
        QString app = url.path(QUrl::FullyDecoded);
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(app);
        cmd.append("sudo emerge =" + app + " --newuse --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(app);

        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));

        composite->window->exec(cmd, QString("%1 install").arg(app));
        composite->window->install(app, isWorld);
    }
    else if(scheme == "uninstall")
    {
        QString app = url.path(QUrl::FullyDecoded);
        QString cmd = "sudo emerge --unmerge =" + app + " --nospinner --noreplace";
        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(app);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));

        composite->window->exec(cmd, QString("%1 uninstall").arg(app));
        composite->window->uninstall(app);
    }
    else if(scheme == "unmask")
    {
        QString app = url.path(QUrl::FullyDecoded);
        QString cmd = "sudo emerge --autounmask --autounmask-write =" + app + " --verbose --verbose-conflicts --nospinner";
        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }
        cmd.append(" && sudo dispatch-conf && sudo emerge =" + app + " --verbose --verbose-conflicts --nospinner");
        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }
        composite->window->exec(cmd, QString("%1 unmask").arg(app));
    }
    else if(scheme == "about")
    {
        currentUrl = url.toString();
        about();
    }
    else if(scheme == "app")
    {
        currentUrl = url.toString();
        viewApp(url);
    }
    else if(scheme == "use")
    {
        currentUrl = url.toString();
        viewUseFlag(url);
    }
    else if(scheme == "file")
    {
        currentUrl = url.toString();
        viewFile(url.toLocalFile());
    }
    else if(scheme == "files")
    {
        currentUrl = url.toString();
        viewAppFiles(url);
    }
    else if(scheme == "new")
    {
        currentUrl = url.toString();
        whatsNew("");
    }
    else if(scheme == "update")
    {
        currentUrl = url.toString();
        QString action = url.path(QUrl::FullyDecoded);
        viewUpdates(action);
    }
}

void BrowserView::reload(bool hardReload)
{
    QPoint pos = saveScrollPosition();
    clear();
    if(currentUrl.isEmpty())
    {
        about();
    }
    else
    {
        composite->delayScroll(pos);
        if(hardReload && currentUrl.startsWith("app:"))
        {
            reloadApp(currentUrl);
        }

        navigateTo(currentUrl, false);
    }
}

void BrowserView::viewApp(const QUrl& url)
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
order by p.PACKAGE, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc
)EOF");

    QString search = url.path(QUrl::FullyDecoded);
    QStringList x = search.split('/');
    QString category = x.first();
    QString package = appNoVersion(x.last());
    QString version = appVersion(x.last());
    int packageId = 0;
    query.bindValue(0, category);
    query.bindValue(1, package);

    QString html;
    bool installed;
    bool obsoleted;
    bool masked;
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
    QString s;
    QString installedSize;
    QString iuse;
    QString useFlags;
    QString cFlags;
    QString cxxFlags;
    QString installDepend;
    QString buildDepend;
    QString runDepend;
    QString postDepend;
    QString lastBuilt;
    QStringList runtimeDeps;
    QStringList optionalDeps;
    QStringList installtimeDeps;
    int firstUnmasked = -1;

    QHash<QString, QString> installedVersions;
    QString versionSize;

    QFile input;

    html = "<HTML>\n";
    html.append(QString("<HEAD><TITLE>%1</TITLE></HEAD>\n<BODY>").arg(package));

    isWorld = false;
    if(query.exec() && query.first())
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
                masked = query.value(13).toBool();
                if(masked == false && keywords.contains(portage->arch))
                {
                    firstUnmasked = query.at();
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
                    query.first();
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

        s = QString("/var/db/pkg/%1/%2-%3/USE").arg(category, package, version);
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly))
        {
            useFlags = input.readAll();
            input.close();
        }

        s = QString("/var/db/pkg/%1/%2-%3/IUSE").arg(category, package, version);
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly))
        {
            iuse = input.readAll();
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

        s = QString("/var/lib/portage/world");
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly))
        {
            QString worldSet = input.readAll();
            input.close();
            if(worldSet.contains(search + "\n"))
            {
                isWorld = true;
            }
        }

        description = query.value(3).toString();
        homePage = query.value(7).toString();
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
        license = query.value(8).toString();
        keywords = query.value(9).toString();
        category = query.value(11).toString();
        package = query.value(12).toString();
        if(hasIcon == false)
        {
            QString cat = category.left(category.indexOf('-'));
            if(iconMap.contains(cat))
            {
                composite->setIcon(iconMap[cat]);
            }
            else
            {
                qDebug() << "Couldn't find" << cat << "in icon map.";
                composite->setIcon(":/img/page.svg");
            }
        }

        if(appicon.isEmpty())
        {
            html.append(QString(R"EOF(<P><B><A HREF="clip:%1">%1</A></B><BR>%2</P><P>%3</P>)EOF").arg(search, description, homePage));
        }
        else
        {
            html.append(QString(R"EOF(
<TABLE>
<TR><TD><IMG SRC="%4"></TD>
<TD><P><BR><B>&nbsp;<A HREF="clip:%5/%1">%5/%1</A></B></P><P>&nbsp;%2</P><P>&nbsp;%3</P></TD></TR>
</TABLE>
)EOF").arg(package, description, homePage, appicon, category));
        }

        html.append("<P><TABLE BORDER=0 CLASS=\"normal\">");
        html.append("<TR><TD></TD><TD><B>Repository&nbsp;&nbsp;</B></TD><TD><B>Slot&nbsp;&nbsp;</B></TD><TD><B>Version&nbsp;&nbsp;</B></TD><TD><B>Status</B>");
        if(isWorld)
        {
            html.append(" (@world member)");
        }
        html.append("</TD></TR>\n");
        html.append("<TR><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD></TR>\n");

        int rowId;
        query.first();
        do
        {
            repo = query.value(0).toString();
            version = query.value(2).toString();
            installed = query.value(4).toBool();
            obsoleted = query.value(5).toBool();
            masked = query.value(13).toBool();
            rowId = query.value(15).toInt();
            slot = query.value(6).toString();
            subslot = query.value(16).toString();
            downloadSize = query.value(14).toInt();
            if(downloadSize != -1)
            {
                installedSize = fileSize(downloadSize);
                installedSize.prepend(", ");
            }
            else
            {
                installedSize.clear();
            }

            if(installed)
            {
                ebuild = QString("/var/db/pkg/%1/%2-%3/%2-%3.ebuild").arg(category, package, version);
            }
            else
            {
                ebuild = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(repo, category, package, version);
            }

            s = query.value(9).toString(); // PACKAGE.KEYWORD
            if(masked)
            {
                s = "masked";
            }
            else if(!s.contains(portage->arch))
            {
                if(s.contains("-*"))
                {
                    s = "broken";
                }
                else
                {
                    s = "unsupported";
                }
            }
            else
            {
                int i = s.indexOf(portage->arch);
                if(i == 0 || s.at(i - 1) == ' ')
                {
                    s = "stable";
                }
                else if(i > 0 && s.at(i-1) == '~')
                {
                    s = "testing";
                }
            }

            s = s + (obsoleted ? " (obsolete" + installedSize + ")" :
                     installed ? " (installed" + installedSize + ")" : "");
            if(installed)
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
            html.append("<TR>");
            if(rowId == packageId)
            {
                html.append("<TD>&#9656;</TD>");
            }
            else
            {
                html.append("<TD></TD>");
            }
            html.append(QString("<TD><A HREF=\"file://%2\">%1</A>&nbsp;&nbsp;</TD>").arg(repo, ebuild));

            if(subslot.isEmpty())
            {
                html.append(QString("<TD>%1&nbsp;&nbsp;</TD>").arg(slot));
            }
            else
            {
                html.append(QString("<TD>%1/%2&nbsp;&nbsp;</TD>").arg(slot, subslot));
            }

            html.append(QString("<TD>%1&nbsp;&nbsp;</TD>").arg(versionSize));
            html.append(QString("<TD>%1</TD>").arg(s));
            html.append("</TR>\n");
        } while(query.next());
        html.append("</TABLE></P>\n");

        if(license.isEmpty() == false)
        {
            html.append(QString("<P><B>License:</B> %1</P>").arg(license));
        }

        QStringList useList;
        QStringList iuseList = iuse.remove("\n").split(' ');
        if(useFlags.isEmpty() == false)
        {
            useList = useFlags.remove("\n").split(' ');
            html.append("<P><B>Applied Flags:</B> ");
            QString s;
            QString flag;
            QString prefix;
            foreach(s, useList)
            {
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

                html.append(QString("%2<A HREF=\"use:%2%1?%3/%4\">%1</A> ").arg(flag, prefix, category, package));
            }
            html.append("</P>\n");
        }

        if(iuse.isEmpty() == false)
        {
            QString s;
            for(int i = 0; i < useList.count(); i++)
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

            iuseList.removeAll("");

            if(iuseList.isEmpty() == false)
            {
                html.append("<P><B>Unused Flags:</B> ");
                QString s;
                QString flag;
                QString prefix;
                foreach(s, iuseList)
                {
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

                    html.append(QString("%2<A HREF=\"use:%2%1?%3/%4\">%1</A> ").arg(flag, prefix, category, package));
                }
                html.append("</P>\n");
            }
        }

        if(lastBuilt.isEmpty() == false)
        {
            QDateTime lastBuiltDT;
            lastBuiltDT.setSecsSinceEpoch(lastBuilt.toInt());
            html.append(QString("<P><B>Last built:</B> %1</P>").arg(lastBuiltDT.toString("dddd MMMM d, yyyy h:mm AP")));
        }

        if(cFlags.isEmpty() == false)
        {
            html.append(QString("<P><B>CFLAGS:</B> %1</P>").arg(cFlags));
        }

        if(cxxFlags.isEmpty() == false && cxxFlags != cFlags)
        {
            html.append(QString("<P><B>CXXFLAGS:</B> %1</P>").arg(cxxFlags));
        }

        if(keywords.isEmpty() == false)
        {
            html.append(QString("<P><B>Keywords:</B> %1</P>").arg(keywords));
        }

        if(runDepend.isEmpty() == false)
        {
            if(installDepend == runDepend)
            {
                html.append("<P><B>Dependencies:</B></P><UL>");
            }
            else
            {
                html.append("<P><B>Dependencies for run-time use:</B></P><UL>");
            }

            runtimeDeps = runDepend.remove("\n").split(' ');
            for(int i = 0; i < runtimeDeps.count(); i++)
            {
                s = portage->linkDependency(runtimeDeps.at(i));
                if(i == 0)
                {
                    html.append(s);
                }
                else
                {
                    html.append(QString("<BR>\n%1").arg(s));
                }
            }

            html.append("</UL>");
        }

        if(postDepend.isEmpty() == false)
        {
            html.append("<P><B>Dependencies not strictly required at run-time:</B></P><UL>");

            optionalDeps = postDepend.remove("\n").split(' ');
            for(int i = 0; i < optionalDeps.count(); i++)
            {
                s = portage->linkDependency(optionalDeps.at(i));
                if(i == 0)
                {
                    html.append(s);
                }
                else
                {
                    html.append(QString("<BR>\n%1").arg(s));
                }
            }

            html.append("</UL>");
        }

        if(installDepend.isEmpty() == false && runDepend != installDepend)
        {
            installtimeDeps = installDepend.remove("\n").split(' ');
            bool first = true;
            for(int i = 0; i < installtimeDeps.count(); i++)
            {
                s = installtimeDeps.at(i);
                if(runtimeDeps.contains(s) == false && optionalDeps.contains(s) == false) // don't show build dependencies that were already listed in the runtime depenencies list
                {
                    if(first)
                    {
                        html.append("<P><B>Dependencies for installing package:</B></P><UL>");
                        html.append(portage->linkDependency(s));
                        first = false;
                    }
                    else
                    {
                        html.append(QString("<BR>\n%1").arg(portage->linkDependency(s)));
                    }
                }
            }

            if(first == false)
            {
                html.append("</UL>");
            }
        }

        if(buildDepend.isEmpty() == false && runDepend != buildDepend)
        {
            QStringList deps = buildDepend.remove("\n").split(' ');
            bool first = true;
            for(int i = 0; i < deps.count(); i++)
            {
                s = deps.at(i);
                if(runtimeDeps.contains(s) == false  && optionalDeps.contains(s) == false && installtimeDeps.contains(s) == false) // don't show build dependencies that were already listed in the runtime/installtime depenencies list
                {
                    if(first)
                    {
                        html.append("<P><B>Dependencies for building from source:</B></P><UL>");
                        html.append(portage->linkDependency(s));
                        first = false;
                    }
                    else
                    {
                        html.append(QString("<BR>\n%1").arg(portage->linkDependency(s)));
                    }
                }
            }

            if(first == false)
            {
                html.append("</UL>");
            }
        }
    }

    html.append("<P>&nbsp;<BR></P>\n");
    html.append("</BODY>\n<HTML>\n");
    QString oldTitle = documentTitle();
    setText(html);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
}

void BrowserView::viewUseFlag(const QUrl& url)
{
    QString useFlag = url.path(QUrl::FullyDecoded);

    composite->setIcon(":/img/flag.svg");

    if(process == nullptr)
    {
        process = new QProcess(this);
    }
    else
    {
        disconnect(process, nullptr, this, nullptr);
    }
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &BrowserView::quseProcessFinished);


    QString flag;

    if(useFlag.startsWith('+') || useFlag.startsWith('-'))
    {
        flag = useFlag.mid(1);
    }
    else
    {
        flag = useFlag;
    }
    QStringList options;
    options << "-C" << "-D" << "-e" << flag;

    if(process->isOpen() == false)
    {
        context = useFlag;
        useApp = url.query();
        process->start("quse", options, QIODevice::ReadWrite);
    }
}

void BrowserView::quseProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    QString flag;
    QString defaultUse;
    if(context.startsWith('+'))
    {
        flag = context.mid(1);
        defaultUse = QString(" (applied by default to %1)").arg(useApp);
    }
    else if(context.startsWith('-'))
    {
        flag = context.mid(1);
        defaultUse = QString(" (disabled by default on %1)").arg(useApp);
    }
    else
    {
        flag = context;
    }

    QString html = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>"%1" flag</TITLE>
<HEAD>
<BODY>
<!--P><PRE><A HREF="clip:%3 %4">%3 %4</A></PRE></P -->
<P><B>"%1" flag%2</B></P>
<P>
)EOF").arg(flag, defaultUse, process->program(), process->arguments().join(' '));

    QString s = process->readAllStandardOutput();
    process->close();

    if(s.isEmpty())
    {
        html.append(QString(R"EOF(
<P>No USE flag descriptions found.</P>
<P>&nbsp;</P>
</BODY>
</HTML>
)EOF"));

        QString oldTitle = documentTitle();
        setHtml(html);
        if(documentTitle() != oldTitle)
        {
            emit titleChanged(documentTitle());

            History::State state;
            state.target = currentUrl;
            state.title = documentTitle();
            emit updateState(state);
        }
        emit loadFinished();
        return;
    }

    QString match;
    QString app;
    int index;

    int i = 2000;
    QStringList lines = s.split("\n");

    // First, we need to re-sort the results of quse because it just splatters the requested app anywhere.
    for(int i = 0; i < lines.count(); i++)
    {
        s = lines.at(i);
        s = s.trimmed();
        if(s.isEmpty())
        {
            continue;
        }

        match = QString("[%1]").arg(flag);
        index = s.indexOf(match);
        if(index >= 0)
        {
            app = s.left(index);
            if(app == "global")
            {
                lines.removeAt(i);
                lines.prepend(s);
            }
            else if(app == useApp)
            {
                lines.removeAt(i);
                if(lines.first().startsWith(QString("global[%1]").arg(flag)))
                {
                    lines.insert(1, s);
                    break;
                }
                else
                {
                    lines.prepend(s);
                    break;
                }
            }
        }
    }

    bool foundFirstOther = false;
    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty())
        {
            continue;
        }

        i--;
        if(i <= 0)
        {
            break;
        }

        match = QString("[%1]").arg(flag);
        index = s.indexOf(match);
        if(index >= 0)
        {
            app = s.left(index);
            s = s.mid(index + match.count());

            if(app == "global")
            {
                html.append(QString("<P>%1</P>\n").arg(s));
            }
            else if(app.contains('/'))
            {
                if(foundFirstOther == false && app != useApp)
                {
                    foundFirstOther = true;
                    html.append(QString("<P><B>Other packages accepting \"%1\" flag:</B></P>\n").arg(flag));
                }
                html.append(QString("<P><A HREF=\"app:%1\">%1</A><BR>%2</P>\n").arg(app, s));
            }
            else
            {
                html.append(QString("<P>%1<BR>%2</P>\n").arg(app, s));
            }
        }
        else
        {
            html.append(QString("<P>%1</P>\n").arg(s));
        }
    }

    if(i <= 0)
    {
        s = QString(R"EOF(
<P>(output exceeds 2000 lines (%1 total), truncated)</P>
<P>&nbsp;</P>
</BODY>
</HTML>
)EOF").arg(lines.count());
    }
    else
    {
        s = QString(R"EOF(
<P>&nbsp;</P>
</BODY>
</HTML>
)EOF");

    }

    html.append(s);

    QString oldTitle = documentTitle();
    setHtml(html);
    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());

        History::State state;
        state.target = currentUrl;
        state.title = documentTitle();
        emit updateState(state);
    }

    emit loadFinished();
}

void BrowserView::qlistProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    QString html = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>%1 Files</TITLE>
<HEAD>
<BODY>
<P><B><A HREF="app:%1">%2</A></B></P>
<P>
)EOF").arg(context, appNoVersion(context));

    QString s = process->readAllStandardOutput();
    process->close();

    QStringList lines = s.split("\n");
    int i = 0;
    foreach(s, lines)
    {
        s = s.trimmed();
        if(s.isEmpty() == false)
        {
            i++;
            if(i > 2000)
            {
                break;
            }
            html.append(QString("<A HREF=\"file://%1\">%1</A><BR>\n").arg(s));
        }
    }

    if(i > 2000)
    {
        s = QString(R"EOF(
<BR>%1 of %2 files</P>
<P>&nbsp;</P>
</BODY>
</HTML>
)EOF").arg(i).arg(lines.count());
    }
    else
    {
        s = QString(R"EOF(
<BR>%1 files</P>
<P>&nbsp;</P>
</BODY>
</HTML>
)EOF").arg(i);

    }

    html.append(s);

    setHtml(html);
    emit loadFinished();
}

void BrowserView::viewAppFiles(const QUrl& url)
{
    QString app = url.path(QUrl::FullyDecoded);

    composite->setIcon(":/img/app.svg");

    if(process == nullptr)
    {
        process = new QProcess(this);
    }
    else
    {
        disconnect(process, nullptr, this, nullptr);
    }

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &BrowserView::qlistProcessFinished);

    QStringList options;
    options << QString("=%1").arg(app);

    if(process->isOpen() == false)
    {
        context = app;
        process->start("qlist", options, QIODevice::ReadWrite);
    }
}

void BrowserView::viewUpdates(QString action)
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

    QString result = "<HTML>\n";
    result.append(QString("<HEAD><TITLE>Update Packages</TITLE></HEAD>\n<BODY>"));

    QSqlQuery query(db);

    QString sql =
        QString(R"EOF(
            select c.CATEGORY, p.PACKAGE, p2.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED, p.KEYWORDS, p.VERSION
            from PACKAGE p
            inner join CATEGORY c on c.CATEGORYID=p.CATEGORYID
            inner join PACKAGE p2 on p2.PACKAGE=p.PACKAGE and p2.CATEGORYID=p.CATEGORYID and p2.SLOT is p.SLOT and p2.PACKAGEID != p.PACKAGEID and p2.INSTALLED=0 and p2.MASKED=0 and
            (
                (p2.V1 > p.V1 or (p.V1 is null and p2.V1 is not null)) or
                (p2.V1 is p.V1 and (p2.V2 > p.V2 or (p.V2 is null and p2.V2 is not null))) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and (p2.V3 > p.V3 or (p.V3 is null and p2.V3 is not null))) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and (p2.V4 > p.V4 or (p.V4 is null and p2.V4 is not null))) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and (p2.V5 > p.V5 or (p.V5 is null and p2.V5 is not null))) or
                (p2.V1 is p.V1 and p2.V2 is p.V2 and p2.V3 is p.V3 and p2.V4 is p.V4 and p2.V5 is p.V5 and (p2.V6 > p.V6 or (p.V6 is null and p2.V6 is not null)))
            )
            where p.INSTALLED != 0 and
            (
                (p.STATUS is null or p.STATUS=0) or
                (p.STATUS=1 and p2.STATUS>=1) or
                (p.STATUS=2 and p2.STATUS=2)
            )
            order by c.CATEGORY, p.PACKAGE, p.MASKED, p2.V1 desc, p2.V2 desc, p2.V3 desc, p2.V4 desc, p2.V5 desc, p2.V6 desc
            limit 10000
            )EOF");

    query.prepare(sql);

    if(action == "fetch")
    {
        fetchUpdates(&query);
    }
    else
    {
        composite->setIcon(":/img/search.svg");
        showUpdates(&query, result);
    }
}

void BrowserView::reloadApp(const QUrl& url)
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

    db.transaction();

    query.prepare("delete from PACKAGE where CATEGORYID=(select CATEGORYID from CATEGORY where CATEGORY=?) and PACKAGE=?");
    QString search = url.path(QUrl::FullyDecoded);
    QStringList x = search.split('/');
    query.bindValue(0, x.first());
    query.bindValue(1, x.last());

    if(query.exec() == false)
    {
        db.rollback();
        return;
    }

    QString category = x.first();
    QString packageName = x.last();
    QStringList packageNameFilter;
    packageNameFilter << QString("%1-*").arg(packageName);
    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString data;
    QString installedFilePath;
    QString repoFilePath;
    QString repo;
    QString slot;
    QString subslot;
    qint64 installed;
    qint64 published;
    QFileInfo fi;
    int downloadSize = -1;
    qint64 pkgstatus = K9Portage::UNKNOWN;
    QString keywords;
    QStringList keywordList;
    QDir dir;
    QDir builds;
    QFile input;
    bool ok;

    int ebuildCount = 0, progressCount = 0;
    foreach(QString repoDir, portage->repos)
    {
        categoryPath = repoDir;
        categoryPath.append(category);
        buildsPath = categoryPath;
        buildsPath.append('/');
        buildsPath.append(packageName);
        builds.setPath(buildsPath);
        builds.setNameFilters(QStringList("*.ebuild"));
        ebuildCount += builds.entryList(QDir::Files).count();
    }

    categoryPath = QString("/var/db/pkg/%1/").arg(category);
    dir.setPath(categoryPath);
    ebuildCount += dir.entryList(packageNameFilter, QDir::Dirs | QDir::NoDotAndDotDot).count();

    QString sql = QString(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6, SLOT,
    LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED, STATUS, SUBSLOT
)
values
(
    (select CATEGORYID from CATEGORY where CATEGORY=?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, 0, ?, ?, ?, 0, ?, ?, ?
)
)EOF");
    query.prepare(sql);
    for(int repoId = 0; repoId < portage->repos.count(); repoId++)
    {
        categoryPath = portage->repos.at(repoId);
        categoryPath.append(category);
///////////////////////////////////////////////////////// start of duplicate code in rescanthread.cpp
        buildsPath = categoryPath;
        buildsPath.append('/');
        buildsPath.append(packageName);
        builds.setPath(buildsPath);
        builds.setNameFilters(QStringList("*.ebuild"));
        foreach(ebuildFilePath, builds.entryList(QDir::Files))
        {
            portage->vars.clear();
            portage->vars["PN"] = packageName;
            portage->setVersion(ebuildFilePath.mid(packageName.length() + 1, ebuildFilePath.length() - (7 + packageName.length() + 1)));

            downloadSize = -1;
            installed = 0;
            installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(category, packageName, portage->version.pvr);
            fi.setFile(installedFilePath);
            if(fi.exists())
            {
                repoFilePath = QString("%1/repository").arg(installedFilePath);
                input.setFileName(repoFilePath);
                if(input.open(QIODevice::ReadOnly))
                {
                    data = input.readAll();
                    input.close();
                    data = data.trimmed();
                    repo = QString("/var/db/repos/%1/").arg(data);
                    if(repo == portage->repos.at(repoId))
                    {
                        installed = fi.birthTime().toSecsSinceEpoch();
                        input.setFileName(QString("%1/SIZE").arg(installedFilePath));
                        if(input.open(QIODevice::ReadOnly))
                        {
                            data = input.readAll();
                            input.close();
                            data = data.trimmed();
                            downloadSize = data.toInt(&ok);
                            if(ok == false)
                            {
                                downloadSize = -1;
                            }
                        }
                    }
                }
            }

            fi.setFile(buildsPath + "/" + ebuildFilePath);
            published = fi.birthTime().toSecsSinceEpoch();

            portage->ebuildReader(buildsPath + "/" + ebuildFilePath);

            keywords = portage->var("KEYWORDS").toString();
            keywordList = keywords.split(' ');
            if(keywordList.contains(portage->arch))
            {
                pkgstatus = K9Portage::STABLE;
            }
            else if(keywordList.contains(QString("~%1").arg(portage->arch)))
            {
                pkgstatus = K9Portage::TESTING;
            }
            else
            {
                pkgstatus = K9Portage::UNKNOWN;
            }

            slot = portage->var("SLOT").toString();
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot.clear();
            }

            query.bindValue(0, category);
            query.bindValue(1, repoId);
            query.bindValue(2, packageName);
            query.bindValue(3, portage->var("DESCRIPTION"));
            query.bindValue(4, portage->var("HOMEPAGE"));
            query.bindValue(5, portage->version.pvr);
            query.bindValue(6, portage->version.cutNoRevision(0));
            query.bindValue(7, portage->version.cutNoRevision(1));
            query.bindValue(8, portage->version.cutNoRevision(2));
            query.bindValue(9, portage->version.cutNoRevision(3));
            query.bindValue(10, portage->version.cutNoRevision(4));
            query.bindValue(11, portage->version.revision());
            query.bindValue(12, slot);
            query.bindValue(13, portage->var("LICENSE"));
            query.bindValue(14, installed);
            query.bindValue(15, downloadSize);
            query.bindValue(16, keywords);
            query.bindValue(17, portage->var("IUSE"));
            query.bindValue(18, published);
            query.bindValue(19, pkgstatus);
            query.bindValue(20, subslot);
            if(query.exec() == false)
            {
                qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                db.rollback();
                return;
            }
            emit loadProgress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(ebuildCount));
            qApp->processEvents();
        }
    }

    bool obsolete;
    int repoId = 0;
    query.prepare(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6,
    SLOT, LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED, STATUS, SUBSLOT
)
values
(
    (select CATEGORYID from CATEGORY where CATEGORY=?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, ?
)
)EOF");

    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");
    QString package;

    categoryPath = QString("/var/db/pkg/%1/").arg(category);
    dir.setPath(categoryPath);
    foreach(package, dir.entryList(packageNameFilter, QDir::Dirs | QDir::NoDotAndDotDot))
    {
        portage->vars.clear();
        portage->vars["PN"] = packageName = package;

        match = pvsplit.match(package, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);
        if(match.hasMatch() && match.lastCapturedIndex() >= 2)
        {
            portage->vars["PN"] = packageName = match.captured(1);
            portage->setVersion(match.captured(2));
        }
        else
        {
            portage->setVersion("0-0-0-0");
        }

        buildsPath = categoryPath;
        buildsPath.append(package);
        repoFilePath = QString("%1/repository").arg(buildsPath);
        input.setFileName(repoFilePath);
        if(!input.open(QIODevice::ReadOnly))
        {
            qDebug() << "Can't open repository file:" << input.fileName();
        }
        else
        {
            data = input.readAll();
            input.close();
            data = data.trimmed();

            installedFilePath = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(data, category, packageName, portage->version.pvr);
            if(QFile::exists(installedFilePath))
            {
                // already imported this package from the repo directory
                emit loadProgress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(ebuildCount));
                qApp->processEvents();
                continue;
            }

            repo = QString("/var/db/repos/%1/").arg(data);
            repoId = -1;
            for(int i = 0; i < portage->repos.count(); i++)
            {
                if(repo == portage->repos.at(i))
                {
                    repoId = i;
                    break;
                }
            }

            downloadSize = -1;
            input.setFileName(QString("%1/SIZE").arg(buildsPath));
            if(input.open(QIODevice::ReadOnly))
            {
                data = input.readAll();
                input.close();
                data = data.trimmed();
                downloadSize = data.toInt(&ok);
                if(ok == false)
                {
                    downloadSize = -1;
                }
            }

            ebuildFilePath = QString("%1/%2.ebuild").arg(buildsPath, package);
            fi.setFile(ebuildFilePath);
            installed = fi.birthTime().toSecsSinceEpoch();
            published = 0;
            obsolete = true;

            portage->ebuildReader(ebuildFilePath);

            keywords = portage->var("KEYWORDS").toString();
            keywordList = keywords.split(' ');
            if(keywordList.contains(portage->arch))
            {
                pkgstatus = K9Portage::STABLE;
            }
            else if(keywordList.contains(QString("~%1").arg(portage->arch)))
            {
                pkgstatus = K9Portage::TESTING;
            }
            else
            {
                pkgstatus = K9Portage::UNKNOWN;
            }

            query.bindValue(0, category);
            if(repoId == -1)
            {
                query.bindValue(1, QVariant(QVariant::Int)); // NULL repoId
            }
            else
            {
                query.bindValue(1, repoId);
            }

            slot = portage->var("SLOT").toString();
            if(slot.contains('/'))
            {
                int ix = slot.indexOf('/');
                subslot = slot.mid(ix + 1);
                slot = slot.left(ix);
            }
            else
            {
                subslot.clear();
            }

            query.bindValue(2, packageName);
            query.bindValue(3, portage->var("DESCRIPTION"));
            query.bindValue(4, portage->var("HOMEPAGE"));
            query.bindValue(5, portage->version.pvr);
            query.bindValue(6, portage->version.cutNoRevision(0));
            query.bindValue(7, portage->version.cutNoRevision(1));
            query.bindValue(8, portage->version.cutNoRevision(2));
            query.bindValue(9, portage->version.cutNoRevision(3));
            query.bindValue(10, portage->version.cutNoRevision(4));
            query.bindValue(11, portage->version.revision());
            query.bindValue(12, slot);
            query.bindValue(13, portage->var("LICENSE"));
            query.bindValue(14, installed);
            query.bindValue(15, obsolete);
            query.bindValue(16, downloadSize);
            query.bindValue(17, keywords);
            query.bindValue(18, portage->var("IUSE"));
            query.bindValue(19, published);
            query.bindValue(20, pkgstatus);
            query.bindValue(21, subslot);
            if(query.exec() == false)
            {
                qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                db.rollback();
                return;
            }
        }

        emit loadProgress(100.0f * static_cast<float>(progressCount++) / static_cast<float>(ebuildCount));
        qApp->processEvents();
    }

    emit loadProgress(98.0f);
    qApp->processEvents();
    db.commit();

    emit loadProgress(99.0f);
    qApp->processEvents();
    portage->applyMasks(db);
}

void BrowserView::searchApps(QString search, bool feelingLucky)
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

    QString result = "<HTML>\n";
    result.append(QString("<HEAD><TITLE>%1 search</TITLE></HEAD>\n<BODY>").arg(search));

    QSqlQuery query(db);
    QString glob = search.replace('*', "%");
    if(search.contains('/'))
    {
        query.prepare("select c.CATEGORY, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED, p.KEYWORDS from PACKAGE p inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID where c.CATEGORY like ? and p.PACKAGE like ? order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc limit 10000");
        QStringList x = glob.split('/');
        query.bindValue(0, "%" + x.first() + "%");
        query.bindValue(1, "%" + x.last() + "%");
    }
    else
    {
        query.prepare("select c.CATEGORY, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED, p.KEYWORDS from PACKAGE p inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID where p.PACKAGE like ? or p.DESCRIPTION like ? or c.CATEGORY=? order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc limit 10000");
        query.bindValue(0, "%" + glob + "%");
        query.bindValue(1, "%" + glob + "%");
        query.bindValue(2, glob);
    }

    composite->setIcon(":/img/search.svg");
    showQueryResult(&query, result, search, feelingLucky);
}

void BrowserView::whatsNew(QString search, bool feelingLucky)
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

    QString result = "<HTML>\n";
    result.append(QString("<HEAD><TITLE>What's New</TITLE></HEAD>\n<BODY>"));

    QSqlQuery query(db);
    query.prepare(R"EOF(
select c.CATEGORY, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED, p.KEYWORDS
from PACKAGE p
inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID
where (p.PACKAGE, p.CATEGORYID) in (select p2.PACKAGE, p2.CATEGORYID from PACKAGE p2 where p2.PUBLISHED > ?)
order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc
limit 10000;
)EOF");
    QDateTime dt = QDateTime::currentDateTime();
    dt = dt.addDays(-3);
    query.bindValue(0, dt.toSecsSinceEpoch());

    composite->setIcon(":/img/new.svg");
    showQueryResult(&query, result, search, feelingLucky);
}

void BrowserView::swipeUpdate()
{
    int newScrollValue = oldScrollValue - dy / 2;
    verticalScrollBar()->setValue(newScrollValue);
    oldScrollValue = newScrollValue;

    if(newScrollValue <= verticalScrollBar()->minimum() ||
       newScrollValue >= verticalScrollBar()->maximum())
    {
        dy = 0;
        animationTimer.stop();
        return;
    }

    if(abs(dy) > 120)
    {
        dy += (dy > 0 ? -8:8);
    }
    else if(abs(dy) > 60)
    {
        dy += (dy > 0 ? -6:6);
    }
    else if(abs(dy) > 20)
    {
        dy += (dy > 0 ? -3:3);
    }
    else
    {
        dy += (dy > 0 ? -1:1);
    }

    if(dy == 0)
    {
        animationTimer.stop();
    }
    else
    {
        int delay = 100 / abs(dy);
        if(delay < 16)
        {
            delay = 16; // 1 / 16ms = 60fps max animation speed
        }
        animationTimer.setInterval(delay);
    }
}

void BrowserView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        animationTimer.stop(); // immediately stop swipe scrolling if user presses screen
        QPointF p = event->pos();
        p.setX(p.x() + horizontalScrollBar()->value());
        p.setY(p.y() + verticalScrollBar()->value());
        int hit = document()->documentLayout()->hitTest(p, Qt::ExactHit);
        if(hit == -1)
        {
            scrollGrabbed = true;
            oldY = event->globalY();
            oldScrollValue = verticalScrollBar()->value();
            return;
        }

        scrollGrabbed = false;
    }

    QString link = anchorAt(event->pos());
    if(link.isEmpty())
    {
        QTextEdit::mousePressEvent(event);
    }
}

void BrowserView::mouseMoveEvent(QMouseEvent* event)
{
    if(scrollGrabbed)
    {
        dy = event->globalY() - oldY;
        if(swiping == false)
        {
            if(abs(dy) < START_SWIPE_THRESHOLD)
            {
                return;
            }

            swiping = true;
            viewport()->setCursor(Qt::ClosedHandCursor);
        }

        int newScrollValue = oldScrollValue - dy;
        verticalScrollBar()->setValue(newScrollValue);
        oldScrollValue = verticalScrollBar()->value();
        if(newScrollValue != oldScrollValue)
        {
            // we've reached the end of the available scroll area
            if(dy > RELOAD_THRESHOLD)
            {
                swiping = false;
                oldY = event->globalY();
            }
        }
        else
        {
            oldY = event->globalY();
        }
        return;
    }

    QPointF p = event->pos();
    p.setX(p.x() + horizontalScrollBar()->value());
    p.setY(p.y() + verticalScrollBar()->value());

    QString link = anchorAt(event->pos());
    int hit = document()->documentLayout()->hitTest(p, Qt::ExactHit);
    if(hit == -1)
    {
        viewport()->unsetCursor();
    }
    else
    {
        if(link.isEmpty())
        {
            viewport()->setCursor(Qt::IBeamCursor);
        }
        else
        {
            viewport()->setCursor(Qt::PointingHandCursor);
        }
    }

    if(link.isEmpty())
    {
        if(oldLink.isEmpty() == false)
        {
            composite->setStatus("");
            oldLink.clear();
        }
    }
    else if(link != oldLink)
    {
        composite->setStatus(link);
        oldLink = link;
    }
    QTextEdit::mouseMoveEvent(event);
}

void BrowserView::mouseReleaseEvent(QMouseEvent* event)
{
    QString link;

    switch(event->button())
    {
        case Qt::BackButton:
            composite->back();
            event->accept();
            return;

        case Qt::ForwardButton:
            composite->forward();
            event->accept();
            return;

        case Qt::LeftButton:
            if(scrollGrabbed)
            {
                scrollGrabbed = false;
                viewport()->unsetCursor();
                event->accept();

                QTextCursor cur = textCursor();
                if(swiping == false)
                {
                    if(cur.hasSelection())
                    {
                        cur.clearSelection();
                        setTextCursor(cur);
                    }
                    QTextEdit::mouseReleaseEvent(event);
                    return;
                }

                if(dy)
                {
                    dy = ((double)dy) * 3.5;
                    animationTimer.setInterval(100 / abs(dy));
                    animationTimer.start();
                }
                swiping = false;
            }
            else
            {
                link = anchorAt(event->pos());
                if(link.isEmpty())
                {
                    QTextEdit::mouseReleaseEvent(event);
                    return;
                }

                composite->navigateTo(link);
                event->accept();
            }
            return;

        case Qt::MiddleButton:
            link = anchorAt(event->pos());
            if(link.isEmpty() || textCursor().hasSelection())
            {
                QTextEdit::mouseReleaseEvent(event);
                return;
            }

            if(link.startsWith("clip:"))
            {
                navigateTo(link);
                if(link.mid(link.indexOf(':')) == currentUrl.mid(currentUrl.indexOf(':')))
                {
                    link = "app" + link.mid(link.indexOf(':'));
                }
                else
                {
                    event->accept();
                    return;
                }
            }

            emit openInNewTab(link);
            event->accept();
            return;

        default:
            QTextEdit::mouseReleaseEvent(event);
            return;
    }
}

void BrowserView::keyPressEvent(QKeyEvent* event)
{
    if(event->modifiers() == (Qt::ControlModifier + Qt::ShiftModifier))
    {
        switch(event->key())
        {
            case Qt::Key_C:
                if(textCursor().hasSelection())
                {
                    QClipboard* clip = qApp->clipboard();
                    clip->setText(textCursor().selectedText());
                }
                return;
        }
    }
    else if(event->modifiers() == Qt::ControlModifier)
    {
        bool ok = false;
        switch(event->key())
        {
            case Qt::Key_F:
                QString search = QInputDialog::getText(this, tr("Find"),
                                                       tr("Find:"), QLineEdit::Normal,
                                                       lastSearch, &ok);
                if(ok && !search.isEmpty())
                {
                    lastSearch = search;
                    QTextCursor cursor = textCursor();
                    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    bool found = find(lastSearch);
                    if(found == false)
                    {
                        composite->setStatus(tr("\"%1\" not found.").arg(lastSearch));
                    }
                }

        }
    }
    else if(event->modifiers() == Qt::ShiftModifier)
    {
        switch(event->key())
        {
            case Qt::Key_F3:
                if(lastSearch.isEmpty() == false)
                {
                    bool found = find(lastSearch, QTextDocument::FindBackward);
                    if(found == false)
                    {
                        QString s = tr("\"%1\" not found.").arg(lastSearch);
                        if(composite->status() == s)
                        {
                            QTextCursor cursor = textCursor();
                            QTextCursor original = cursor;
                            QPoint originalPos = scrollPosition();
                            cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                            setTextCursor(cursor);
                            bool found = find(lastSearch, QTextDocument::FindBackward);
                            if(found == true)
                            {
                                s = tr("Search wrapped to end of page.");
                            }
                            else
                            {
                                setTextCursor(original);
                                setScrollPosition(originalPos);;
                            }
                        }
                        composite->setStatus(s);
                    }
                    else if(composite->status() == tr("Search wrapped to end of page."))
                    {
                        composite->setStatus("");
                    }
                }
        }
    }
    else if(event->modifiers() == Qt::NoModifier)
    {
        switch(event->key())
        {
            case Qt::Key_F3:
                if(lastSearch.isEmpty() == false)
                {
                    bool found  = find(lastSearch);
                    if(found == false)
                    {
                        QString s = tr("\"%1\" not found.").arg(lastSearch);
                        if(composite->status() == s)
                        {
                            QTextCursor cursor = textCursor();
                            QTextCursor original = cursor;
                            QPoint originalPos = scrollPosition();
                            cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                            setTextCursor(cursor);
                            bool found = find(lastSearch);
                            if(found == true)
                            {
                                s = tr("Search wrapped to beginning of page.");
                            }
                            else
                            {
                                setTextCursor(original);
                                setScrollPosition(originalPos);
                            }
                        }
                        composite->setStatus(s);
                    }
                    else if(composite->status() == tr("Search wrapped to beginning of page."))
                    {
                        composite->setStatus("");
                    }
                }
        }
    }

    QTextEdit::keyPressEvent(event);
}

void BrowserView::contextMenuUninstallLink(QMenu* menu, QString urlPath)
{
    QAction* action;
    action = new QAction("Copy atom to clipboard", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(urlPath);
    });
    menu->addAction(action);

    action = new QAction("Verify integrity", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        shell->externalTerm(QString("qcheck =%1").arg(urlPath), QString("%1 integrity check").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("List files owned", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        navigateTo(QString("files:%1").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Who depends on this?", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        shell->externalTerm(QString("equery depends =%1").arg(urlPath), QString("%1 needed by").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Dependencies graph", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QUrl url(context);
        QString app = url.path(QUrl::FullyDecoded);
        shell->externalTerm(QString("equery depgraph =%1  | less").arg(app), QString("%1 dependencies graph").arg(app));
    });
    menu->addAction(action);

    action = new QAction("View size", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QUrl url(context);
        QString app = url.path(QUrl::FullyDecoded);
        shell->externalTerm(QString("qsize =%1").arg(app), QString("%1 size").arg(app));
    });
    menu->addAction(action);

    action = new QAction("Reinstall", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo emerge =" + urlPath + " --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(urlPath);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 reinstall").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Reinstall from source", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(urlPath);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 reinstall from source").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Reinstall && rebuild reverse dependencies", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString appPathName = appNoVersion(urlPath);
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append(QString("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =%1 `qdepends -QqqC -F \"=%[CATEGORY]%[PF]\" ^%2 | tr '\\n' ' '` --usepkg=n --verbose --verbose-conflicts --nospinner").arg(urlPath, appPathName));
        cmd.append(" --oneshot");

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 install w/reverse dep rebuild").arg(urlPath));
    });
    menu->addAction(action);
}

void BrowserView::contextMenuInstallLink(QMenu* menu, QString urlPath)
{
    QAction* action;
    action = new QAction("Copy atom to clipboard", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(urlPath);
    });
    menu->addAction(action);

    action = new QAction("Install && rebuild reverse dependencies", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString appPathName = appNoVersion(urlPath);
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append(QString("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =%1 `qdepends -QqqC -F \"=%[CATEGORY]%[PF]\" ^%2 | tr '\\n' ' '` --usepkg=n --verbose --verbose-conflicts --nospinner").arg(urlPath, appPathName));
        cmd.append(" --oneshot");

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 install w/reverse dep rebuild").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Install from source", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(urlPath);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 install from source").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Build binary package", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo emerge =" + urlPath + " --buildpkgonly --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }
        composite->window->exec(cmd, QString("%1 build binary package only").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Fetch", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd.append("sudo emerge =" + urlPath + " --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner\n");
        composite->window->exec(cmd, QString("%1 fetch").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Fetch source", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("sudo sh -c \"echo '=%1 **' >> /etc/portage/package.accept_keywords/appswipe.tmp\"\n").arg(urlPath);
        cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner\n");
        composite->window->exec(cmd, QString("%1 fetch source").arg(urlPath));
    });
    menu->addAction(action);
}

void BrowserView::contextMenuAppLink(QMenu* menu, QString urlPath)
{
    QAction* action;
    action = new QAction("Copy atom to clipboard", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(urlPath);
    });
    menu->addAction(action);

    action = new QAction("Open link in new tab", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        CompositeView* view = composite->window->tabWidget()->createTab();
        view->navigateTo(context);
        view->setFocus();
    });
    menu->addAction(action);

    action = new QAction("Open link in new window", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        BrowserWindow* newWindow = composite->window->openWindow();
        newWindow->currentView()->navigateTo(context);
        newWindow->currentView()->setFocus();
    });
    menu->addAction(action);

    action = new QAction("Install", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo emerge =" + urlPath + " --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(urlPath);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 install").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Install from source", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("qlop -Hp =%1\n").arg(urlPath);
        cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner");
        if(isWorld == false)
        {
            cmd.append(" --oneshot");
        }

        if(composite->window->ask)
        {
            cmd.append(" --ask");
        }

        QString appPathName = appNoVersion(urlPath);
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -emerged %2 -pid %3").arg(qApp->applicationFilePath(), appPathName).arg(qApp->applicationPid()));
        composite->window->exec(cmd, QString("%1 install from source").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Fetch", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("sudo sh -c \"echo '=%1 **' >> /etc/portage/package.accept_keywords/appswipe.tmp\"\n").arg(urlPath);
        cmd.append("sudo emerge " + urlPath + " --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner\n");
        //cmd.append("sudo rm -f \"/etc/portage/package.accept_keywords/appswipe.tmp\"");
        composite->window->exec(cmd, QString("%1 fetch").arg(urlPath));
    });
    menu->addAction(action);

    action = new QAction("Fetch source", this);
    connect(action, &QAction::triggered, this, [this, urlPath]()
    {
        QString cmd;
        cmd = QString("sudo sh -c \"echo '=%1 **' >>  /etc/portage/package.accept_keywords/appswipe.tmp\"\n").arg(urlPath);
        cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge " + urlPath + " --usepkg=n --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner\n");
        //cmd.append("sudo rm -f \"/etc/portage/package.accept_keywords/appswipe.tmp\"");
        composite->window->exec(cmd, QString("%1 fetch source").arg(urlPath));
    });
    menu->addAction(action);
}

void BrowserView::contextMenuAppWhitespace(QMenu* menu)
{
    QAction* action;
    QString urlPath = composite->url().mid(4);

    action = new QAction("Copy atom to clipboard", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(urlPath);
    });
    menu->addAction(action);

    action = new QAction("Who depends on this?", this);
    connect(action, &QAction::triggered, this, [urlPath]()
    {
        shell->externalTerm(QString("qdepends --query --quiet --installed %1").arg(urlPath), QString("%1 needed by").arg(urlPath));
    });
    menu->addAction(action);

    if(isWorld == false)
    {
        action = new QAction("Add to @world set", this);
        connect(action, &QAction::triggered, this, [this, urlPath]()
        {
            QString cmd = "sudo emerge --noreplace " + urlPath + " --verbose --verbose-conflicts --nospinner --ask=n";
            cmd.append(QString("\nexport RET_CODE=$?\n%1 -pid %2").arg(qApp->applicationFilePath()).arg(qApp->applicationPid()));
            shell->externalTerm(cmd, QString("%1 add to @world").arg(urlPath), false);
            isWorld = true;
        });
        menu->addAction(action);
    }
    else
    {
        action = new QAction("Remove from @world set", this);
        connect(action, &QAction::triggered, this, [this, urlPath]()
        {
            QString cmd = "sudo emerge --deselect " + urlPath + " --verbose --verbose-conflicts --nospinner --ask=n";
            cmd.append(QString("\nexport RET_CODE=$?\n%1 -pid %2").arg(qApp->applicationFilePath()).arg(qApp->applicationPid()));
            shell->externalTerm(cmd, QString("%1 remove from @world").arg(urlPath), false);
            isWorld = false;
        });
        menu->addAction(action);
    }
}

void BrowserView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = nullptr;
    QAction* action;

    context = anchorAt(event->pos());
    if(context.isEmpty())
    {
        if(composite->url().startsWith("app:"))
        {
            menu = new QMenu(this);
            contextMenuAppWhitespace(menu);
        }
    }
    else
    {
        menu = new QMenu(this);

        QUrl contextUrl(context);
        QString urlPath = contextUrl.path(QUrl::FullyDecoded);

        if(context.startsWith("http"))
        {
            action = new QAction("Copy link address", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(context);
            });
            menu->addAction(action);

            action = new QAction("Open link in external browser", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                shell->externalBrowser(context);
            });
            menu->addAction(action);
        }
        else if(context.startsWith("file://"))
        {
            action = new QAction("Copy path to clipboard", this);
            connect(action, &QAction::triggered, this, [urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);

            action = new QAction("Open link in new tab", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                CompositeView* view = composite->window->tabWidget()->createTab();
                view->navigateTo(context);
                view->setFocus();
            });
            menu->addAction(action);

            action = new QAction("Open link in new window", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                BrowserWindow* newWindow = composite->window->openWindow();
                newWindow->currentView()->navigateTo(context);
                newWindow->currentView()->setFocus();
            });
            menu->addAction(action);

            QFileInfo fi(urlPath);
            if(fi.isDir())
            {
                action = new QAction("Open folder externally...", this);
            }
            else
            {
                action = new QAction("Open file externally...", this);
            }
            connect(action, &QAction::triggered, this, [this]()
            {
                shell->externalFileManager(context);
            });
            menu->addAction(action);

            if(urlPath.startsWith("/var/db/repos/") && urlPath.endsWith(".ebuild"))
            {
                action = new QAction("Fetch source (ignoring dependencies)", this);
                connect(action, &QAction::triggered, this, [this, urlPath]()
                {
                    QString cmd;
                    cmd.append("sudo FEATURES=\"${FEATURES} -getbinpkg\" ebuild " + urlPath + " fetch\n");
                    composite->window->exec(cmd, QString("%1 fetch source (ignoring dependencies)").arg(urlPath));
                });
                menu->addAction(action);
            }
        }
        else if(context.startsWith("unmask:"))
        {
            QAction* action;
            action = new QAction("Copy atom to clipboard", this);
            connect(action, &QAction::triggered, this, [urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);
        }
        else if(context.startsWith("uninstall:"))
        {
            contextMenuUninstallLink(menu, urlPath);
        }
        else if(context.startsWith("install:"))
        {
            contextMenuInstallLink(menu, urlPath);
        }
        else if(context.startsWith("app:"))
        {
            contextMenuAppLink(menu, urlPath);
        }
        else
        {
            delete menu;
            menu = nullptr;
        }
    }

    if(menu != nullptr)
    {
        menu->exec(event->globalPos());
        delete menu;
    }
}

QString BrowserView::appNoVersion(QString app)
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

QString BrowserView::appVersion(QString app)
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

void BrowserView::fetchUpdates(QSqlQuery* query)
{
    QString category;
    QString app;
    QString package;
    QString version;
    QString description;
    QString keywords;
    QString nextCategory;
    QString nextPackage;

    QString bestMatchApps;
    QString obsoletedApps;
    QString installedApps;
    QString availableApps;

    bool installed;
    bool obsoleted;
    bool masked;
    int appCount = 0;

    QString latestUnmaskedVersion;
    QStringList obsoletedVersions;
    QStringList installedVersions;
    QString installedVersion;
    QStringList packagesPayload;
    bool exitLoop = false;

    if(query->exec() == false || query->first() == false)
    {
        return;
    }
    category = query->value(0).toString();
    package = query->value(1).toString();
    installedVersions.clear();
    obsoletedVersions.clear();
    latestUnmaskedVersion.clear();

    while(exitLoop == false)
    {
        version = query->value(2).toString();
        installedVersion = query->value(8).toString();
        if(installedVersions.isEmpty())
        {
            description = query->value(3).toString();
        }
        installed = (query->value(4).toInt() != 0);
        masked = (query->value(5).toInt() != 0);
        obsoleted = (query->value(6).toInt() != 0);
        keywords = query->value(7).toString();
        if(((masked == false && keywords.contains(portage->arch)) || installed) &&
           latestUnmaskedVersion.isEmpty())
        {
            latestUnmaskedVersion = version;
        }

        if(obsoleted)
        {
            obsoletedVersions.append(installedVersion);
        }
        else if(installed)
        {
            installedVersions.append(installedVersion);
        }

        if(query->next() == false)
        {
            exitLoop = true;
        }
        else
        {
            nextCategory = query->value(0).toString();
            nextPackage = query->value(1).toString();
        }

        if(exitLoop || nextCategory != category || nextPackage != package)
        {
            if(package.isEmpty() == false)
            {
                appCount++;
                app = QString("%1/%2").arg(category, package);
                packagesPayload.append(QString("=%1/%2-%3").arg(category, package, latestUnmaskedVersion));
            }

            category = nextCategory;
            package = nextPackage;
            installedVersions.clear();
            obsoletedVersions.clear();
            latestUnmaskedVersion.clear();
        }
    }

    if(appCount == 0)
    {
        return;
    }

    QString cmd = "sudo emerge --autounmask --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner ";
    if(composite->window->ask)
    {
        cmd.append("--ask ");
    }

    cmd.append(packagesPayload.join(' '));
    composite->window->exec(cmd, QString("Fetch %1 Updates").arg(appCount));
}

void BrowserView::showUpdates(QSqlQuery* query, QString result)
{
    QString category;
    QString app;
    QString package;
    QString version;
    QString description;
    QString keywords;
    int bestYet = -1;
    QString bestCategory;
    QString bestPackage;
    QString nextCategory;
    QString nextPackage;

    QString bestMatchApps;
    QString obsoletedApps;
    QString installedApps;
    QString availableApps;

    bool installed;
    bool obsoleted;
    bool masked;
    bool tooManyResults = false;
    int appCount = 0;

    QString latestUnmaskedVersion;
    QStringList obsoletedVersions;
    QStringList installedVersions;
    QString installedVersion;
    QString packagesPayload;
    bool exitLoop = false;

    if(query->exec() == false || query->first() == false)
    {
        error("No results found.");
        return;
    }
    category = query->value(0).toString();
    package = query->value(1).toString();
    installedVersions.clear();
    obsoletedVersions.clear();
    latestUnmaskedVersion.clear();

    while(exitLoop == false)
    {
        version = query->value(2).toString();
        installedVersion = query->value(8).toString();
        if(installedVersions.isEmpty())
        {
            description = query->value(3).toString();
        }
        installed = (query->value(4).toInt() != 0);
        masked = (query->value(5).toInt() != 0);
        obsoleted = (query->value(6).toInt() != 0);
        keywords = query->value(7).toString();
        if(((masked == false && keywords.contains(portage->arch)) || installed) &&
           latestUnmaskedVersion.isEmpty())
        {
            latestUnmaskedVersion = version;
        }

        if(obsoleted && installedVersions.contains(installedVersion) == false && obsoletedVersions.contains(installedVersion) == false)
        {
            obsoletedVersions.append(installedVersion);
        }
        else if(installed && installedVersions.contains(installedVersion) == false && obsoletedVersions.contains(installedVersion) == false)
        {
            installedVersions.append(installedVersion);
        }

        if(query->next() == false)
        {
            exitLoop = true;
        }
        else
        {
            nextCategory = query->value(0).toString();
            nextPackage = query->value(1).toString();
        }

        if(exitLoop || nextCategory != category || nextPackage != package)
        {
            if(package.isEmpty() == false)
            {
                appCount++;
                app = QString("%1/%2").arg(category, package);
                packagesPayload.append(QString("=%1/%2-%3 ").arg(category, package, latestUnmaskedVersion));

                if(app == "sys-apps/portage")
                {
                    bestCategory = category;
                    bestPackage = package;
                    bestYet = 100;
                    printApp(bestMatchApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else if(obsoletedVersions.count() || (installedVersions.count() && (installedVersions.contains(latestUnmaskedVersion) == false)))
                {
                    if(bestYet < 50)
                    {
                        bestYet = 50;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(obsoletedApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else if(installedVersions.count())
                {
                    if(bestYet < 25)
                    {
                        bestYet = 25;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(installedApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else
                {
                    if(bestYet < 5)
                    {
                        bestYet = 5;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(availableApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }

                if(appCount > 9999)
                {
                    tooManyResults = true;
                    break;
                }
            }

            category = nextCategory;
            package = nextPackage;
            installedVersions.clear();
            obsoletedVersions.clear();
            latestUnmaskedVersion.clear();
        }
    }

    if(appCount == 0)
    {
        error("No results found.");
        return;
    }

    result.append(QString("<P>%1 upgradable packages found. <B><A HREF=\"update:fetch\">Fetch All</A></B></P>\n").arg(appCount));

    if(appCount == 1)
    {
        QString target = QString("app:%1/%2").arg(bestCategory, bestPackage);
        emit urlChanged(target);
        viewApp(target);

        History::State state;
        state.target = target;
        state.title = composite->title();
        emit updateState(state);
        return;
    }

    result.append(bestMatchApps);
    result.append(obsoletedApps);
    result.append(installedApps);
    result.append(availableApps);

    if(tooManyResults)
    {
        result.append("<P>Further results truncated...</P>\n");
    }
    result.append("\n");
    result.append("<P>&nbsp;<BR></P>\n");
    result.append("</BODY>\n<HTML>\n");

    QString oldTitle = documentTitle();
    setHtml(result);
    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }

    setFocus();
}

void BrowserView::showQueryResult(QSqlQuery* query, QString result, QString search, bool feelingLucky)
{
    QString category;
    QString app;
    QString package;
    QString version;
    QString description;
    QString keywords;
    int bestYet = -1;
    QString bestCategory;
    QString bestPackage;
    QString nextCategory;
    QString nextPackage;

    QString bestMatchApps;
    QString obsoletedApps;
    QString installedApps;
    QString availableApps;

    bool installed;
    bool obsoleted;
    bool bestMatched = false;
    bool masked;
    bool tooManyResults = false;
    int appCount = 0;

    QString latestUnmaskedVersion;
    QStringList obsoletedVersions;
    QStringList installedVersions;
    bool exitLoop = false;

    if(query->exec() == false || query->first() == false)
    {
        error("No results found.");
        return;
    }
    category = query->value(0).toString();
    package = query->value(1).toString();
    installedVersions.clear();
    obsoletedVersions.clear();
    latestUnmaskedVersion.clear();
    bestMatched = false;
    if(search.contains('/') == false && package == search)
    {
        bestMatched = true;
    }

    while(exitLoop == false)
    {
        version = query->value(2).toString();
        if(installedVersions.isEmpty())
        {
            description = query->value(3).toString();
        }
        installed = (query->value(4).toInt() != 0);
        masked = (query->value(5).toInt() != 0);
        obsoleted = (query->value(6).toInt() != 0);
        keywords = query->value(7).toString();
        if(((masked == false && keywords.contains(portage->arch)) || installed) &&
           latestUnmaskedVersion.isEmpty())
        {
            latestUnmaskedVersion = version;
        }

        if(obsoleted)
        {
            obsoletedVersions.append(version);
        }
        else if(installed)
        {
            installedVersions.append(version);
        }

        if(query->next() == false)
        {
            exitLoop = true;
        }
        else
        {
            nextCategory = query->value(0).toString();
            nextPackage = query->value(1).toString();
        }

        if(exitLoop || nextCategory != category || nextPackage != package)
        {
            if(package.isEmpty() == false)
            {
                appCount++;
                app = QString("%1/%2").arg(category, package);
                if(bestMatched)
                {
                    bestCategory = category;
                    bestPackage = package;
                    bestYet = 100;
                    if(feelingLucky)
                    {
                        break;
                    }
                    printApp(bestMatchApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else if(obsoletedVersions.count() || (installedVersions.count() && (installedVersions.contains(latestUnmaskedVersion) == false)))
                {
                    if(bestYet < 50)
                    {
                        bestYet = 50;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(obsoletedApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else if(installedVersions.count())
                {
                    if(bestYet < 25)
                    {
                        bestYet = 25;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(installedApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }
                else
                {
                    if(bestYet < 5)
                    {
                        bestYet = 5;
                        bestPackage = package;
                        bestCategory = category;
                    }
                    printApp(availableApps, app, description, latestUnmaskedVersion, installedVersions, obsoletedVersions);
                }

                if(appCount > 9999)
                {
                    tooManyResults = true;
                    break;
                }
            }

            category = nextCategory;
            package = nextPackage;
            installedVersions.clear();
            obsoletedVersions.clear();
            latestUnmaskedVersion.clear();
            bestMatched = false;
            if(search.contains('/') == false && package == search)
            {
                bestMatched = true;
            }
        }
    }

    if(appCount == 0)
    {
        error("No results found.");
        return;
    }

    if(appCount == 1 || feelingLucky)
    {
        currentUrl = QString("app:%1/%2").arg(bestCategory, bestPackage);
        emit urlChanged(currentUrl);
        viewApp(currentUrl);
        return;
    }

    result.append(bestMatchApps);
    result.append(obsoletedApps);
    result.append(installedApps);
    result.append(availableApps);

    if(tooManyResults)
    {
        result.append("<P>Further search results truncated...</P>\n");
    }
    result.append("\n");
    result.append("<P>&nbsp;</P>\n");
    result.append("</BODY>\n<HTML>\n");

    QString oldTitle = documentTitle();
    setText(result);
    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }

    setFocus();
}

void BrowserView::printApp(QString& result, QString& app, QString& description, QString& latestVersion, QStringList& installedVersions, QStringList& obsoletedVersions)
{
    int i;

    QString obsoleted;
    QString installed;
    for(i = 0; i < installedVersions.count(); i++)
    {
        if(i == 0)
        {
            installed = installedVersions.at(0);
        }
        else
        {
            installed.append(QString(", %1").arg(installedVersions.at(i)));
        }
    }

    for(i = 0; i < obsoletedVersions.count(); i++)
    {
        if(i == 0)
        {
            obsoleted = obsoletedVersions.at(0);
        }
        else
        {
            obsoleted.append(QString(", %1").arg(obsoletedVersions.at(i)));
        }
    }

    if(installedVersions.isEmpty() && obsoletedVersions.isEmpty())
    {
        // no versions of this app have been installed
        result.append(QString("<P><A HREF=\"app:%1\">%1</A><BR>").arg(app));
    }
    else
    {
        result.append(QString("<P><B><A HREF=\"app:%1\">%1-%2</A> (").arg(app, latestVersion));

        if(installedVersions.contains(latestVersion))
        {
            if(obsoletedVersions.isEmpty())
            {
                // latest version of app is installed, no obsoleted versions.
                if(installedVersions.count() == 1)
                {
                    result.append(QString("installed)</B><BR>"));
                }
                else
                {
                    result.append(QString("installed %1)</B><BR>").arg(installed));
                }
            }
            else
            {
                // latest version of app is installed, along with one or more obsoleted versions.
                if(installedVersions.count() == 1)
                {
                    result.append(QString("installed, obsolete %1)</B><BR>").arg(obsoleted));
                }
                else
                {
                    result.append(QString("installed %1, obsolete %2)</B><BR>").arg(installed, obsoleted));
                }
            }
        }
        else
        {
            if(installedVersions.isEmpty())
            {
                // only obsoleted version(s) are installed
                result.append(QString("obsolete %1)</B><BR>").arg(obsoleted));
            }
            else
            {
                if(obsoletedVersions.isEmpty())
                {
                    // no obsoleted versions installed
                    result.append(QString("installed %1)</B><BR>").arg(installed));
                }
                else
                {
                    // both supported and obsoleted versions installed
                    result.append(QString("installed %1, obsolete %2)</B><BR>").arg(installed, obsoleted));
                }
            }
        }
    }

    if(description.isEmpty())
    {
        description = "(no description available)";
    }
    result.append(QString("%1</P>").arg(description));
}

QString BrowserView::findAppIcon(bool& hasIcon, QString category, QString package, QString version)
{
    QFile input;
    QStringList bigIcons;
    QStringList smallIcons;
    QStringList desktopFiles;
    QString s;
    QString appicon = QString("/var/db/pkg/%1/%2-%3/CONTENTS").arg(category, package, version);
    input.setFileName(appicon);
    if(input.open(QIODevice::ReadOnly))
    {
        s = input.readAll();
        input.close();
        QStringList lines = s.split('\n');
        foreach(s, lines)
        {
            s.remove('\n');
            s = s.trimmed();
            if(s.isEmpty() || s.startsWith('#'))
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
            composite->setIcon(smallIcons.first());
            hasIcon = true;
        }
    }

    return appicon;
}

void BrowserView::about()
{
    composite->setIcon(":/img/appicon.svg");

    QString text = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>About</TITLE>
<HEAD>
<BODY>
<IMG SRC=":/img/appicon.svg" HEIGHT=297 ALIGN=RIGHT><P><FONT SIZE=+2>%1 v%2</FONT><BR>
%3</P>

<P>This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.</P>

<P>This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.</P>

<P>You should have received a copy of the GNU General Public License
along with this program; if not, write to:<BR>
<UL>
Free Software Foundation, Inc.<BR>
59 Temple Place - Suite 330<BR>
Boston, MA 02111-1307<BR>
USA.</UL></P>

<P>&nbsp;<BR></P>
</BODY>
</HTML>
)EOF").arg(APP_NAME, APP_VERSION, APP_HTMLCOPYRIGHT);
    QString oldTitle = documentTitle();
    setText(text);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
    setFocus();
}

QPoint BrowserView::saveScrollPosition()
{
    History::State state;
    state.target = composite->url();
    state.title = composite->title();
    state.pos = scrollPosition();
    emit updateState(state);

    return state.pos;
}

void BrowserView::reloadingDatabase()
{
    composite->setIcon(":/img/appicon.svg");

    QString text = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>Loading</TITLE>
<HEAD>
<BODY>
<P ALIGN=CENTER><IMG SRC=":/img/appicon.svg"></P>
</BODY>
</HTML>
)EOF");
    QString oldTitle = documentTitle();
    setText(text);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
}

void BrowserView::viewFile(QString fileName)
{
    QFile input(fileName);
    if(!input.exists())
    {
        error("File '" + fileName + "' does not exist.");
        return;
    }

    QFileInfo fi(fileName);
    if(fi.isDir())
    {
        viewFolder(fileName);
        return;
    }

    if(!input.open(QIODevice::ReadOnly))
    {
        error("File '" + fileName + "' could not be opened.");
        return;
    }

    composite->setIcon(":/img/page.svg");
    QByteArray data = input.readAll();
    QString oldTitle = documentTitle();
    if(fileName.endsWith(".md"))
    {
        setMarkdown(data);
    }
    else if(fileName.endsWith(".html") || fileName.endsWith(".htm"))
    {
        setHtml(data);
    }
    else
    {
        QFont font("DejaVu Sans Mono", 12);
        setCurrentFont(font);
        setTabStopDistance(40);
        document()->clear();
        document()->setPlainText(data);
    }

    if(documentTitle().isEmpty())
    {
        setDocumentTitle(fi.fileName());
    }

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
    setFocus();
}

void BrowserView::viewFolder(QString folderPath)
{
    composite->setIcon(":/img/folder.svg");
    QDir dir;
    if(folderPath.endsWith('/') == false)
    {
        folderPath.append('/');
    }

    if(folderPath.startsWith("//"))
    {
        folderPath = folderPath.mid(1);
    }

    dir.setPath(folderPath);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QString text = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>%1</TITLE>
<HEAD>
<BODY>
<P><B><A HREF="clip:%1">%1</A></B><BR></P>
<TABLE BORDER=0 CLASS="normal">
<TR><TD COLSPAN=2>&nbsp;&nbsp;Name</TD><TD></TD><TD>Size</TD><TD></TD><TD>Date Modified</TD></TR>
<TR><TD WIDTH=34><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD></TR>
)EOF").arg(folderPath);
    QFileInfo fi;
    QString dateFormat = "yyyy-MM-dd hh:mm:ss";
    int i = 0;
    foreach(QString fileName, dir.entryList(QDir::Dirs | QDir::NoDot))
    {
        if(++i > 2000)
        {
            break;
        }

        fi.setFile(folderPath + fileName);
        if(fileName == "..")
        {
            if(folderPath != "/")
            {
                text.append(QString("<TR><TD><CENTER><A HREF=\"file://%1/\"><IMG SRC=\":img/folder.svg\" HEIGHT=32></A></CENTER></TD><TD><A HREF=\"file://%1/\">Parent Folder</A></TD><TD></TD><TD></TD><TD></TD><TD>%2</TD></TR>\n").arg(fi.canonicalFilePath(), fi.lastModified().toString(dateFormat)));
            }
        }
        else
        {
            text.append(QString("<TR><TD><CENTER><A HREF=\"file://%1/\"><IMG SRC=\":img/folder.svg\" HEIGHT=32></A></CENTER></TD><TD><A HREF=\"file://%1/\">%2/</A></TD><TD></TD><TD></TD><TD></TD><TD>%3</TD></TR>\n").arg(fi.canonicalFilePath(), fileName, fi.lastModified().toString(dateFormat)));
        }
    }

    foreach(QString fileName, dir.entryList(QDir::Files | QDir::NoDotAndDotDot))
    {
        if(++i > 2000)
        {
            break;
        }

        fi.setFile(folderPath + fileName);
        text.append(QString("<TR><TD><CENTER><A HREF=\"file://%1\"><IMG SRC=\":img/document.svg\" HEIGHT=26></A></CENTER></TD><TD><A HREF=\"file://%1\">%2</A></TD><TD></TD><TD ALIGN=RIGHT>%3</TD><TD></TD><TD>%4</TD></TR>\n").arg(fi.canonicalFilePath(), fi.fileName(), fileSize(fi.size()), fi.lastModified().toString(dateFormat)));
    }

    text.append(QString(R"EOF(
</TABLE>
<P>&nbsp;<BR></P>
</BODY>
</HTML>
)EOF"));


    QString oldTitle = documentTitle();
    setHtml(text);
    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
    setFocus();
}

void BrowserView::error(QString text)
{
    QString oldTitle = documentTitle();
    QString html = QString(R"EOF(
<HTML>
<HEAD><TITLE>Oops</TITLE></HEAD>
<BODY>
<P><IMG SRC=":/img/kytka1.svg" ALIGN=LEFT><FONT SIZE=+2>Oops !</FONT></P>
<P>%1</P>
</BODY></HTML>
)EOF").arg(text);

    setText(html);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }

    composite->window->focusLineEdit();
}

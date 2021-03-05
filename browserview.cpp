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

#include "browserview.h"
#include "datastorage.h"
#include "k9portage.h"
#include "globals.h"
#include "k9shell.h"
#include "browserwindow.h"
#include "main.h"
#include "versionstring.h"
#include "tabwidget.h"

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
#include <QProgressBar>
#include <QResizeEvent>
#include <QDateTime>

#define PROGRESSBAR_HEIGHT 24

BrowserView::BrowserView(QWidget *parent) : QTextEdit(parent)
{
    progress = nullptr;
    status = new QLabel(this);
    status->setStyleSheet(R"EOF(
padding-left: 1px;
padding-right: 1px;
background-color: rgb(57, 57, 57);
border-top-right-radius: 8px;
border-top-left-radius: 8px;
)EOF");
    status->setVisible(false);

    document()->setDefaultStyleSheet(R"EOF(
                                     a { color: rgb(181, 229, 229); }

                                     table.normal
                                     {
                                         color: white;
                                         border-style: none;
                                         background-color: #101A20;
                                     }
)EOF");

    delayLoading = false;
    currentHistory = -1;
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
}

QMenu* BrowserView::createStandardContextMenu(const QPoint& position)
{
    QMenu* menu = nullptr;
    QAction* action;

    context = anchorAt(position);
    if(context.isEmpty() == false)
    {
        menu = new QMenu("Browser Menu", this);

        action = new QAction("Browse Folder", this);
        connect(action, &QAction::triggered, this, [this]()
        {
            qDebug() << "Browse folder:" << context;
        });
        menu->addAction(action);
    }

    return menu;
}

void BrowserView::appendHistory(QString text, int scrollX, int scrollY)
{
    while(currentHistory < history.count() - 1)
    {
        history.removeLast();
    }

    History item;
    if(currentHistory >= 0 && currentHistory < history.count())
    {
        item = history.at(currentHistory);
        item.scrollX = scrollX;
        item.scrollY = scrollY;
        history.replace(currentHistory, item);
    }

    item.target = text;
    item.scrollX = 0;
    item.scrollY = 0;

    history.append(item);
    currentHistory++;
}

QString BrowserView::url()
{
    if(delayLoading)
    {
        return delayUrl;
    }

    QString url;
    if(currentHistory >= 0 && currentHistory < history.count())
    {
        url = history.at(currentHistory).target;
    }
    return url;
}

QString BrowserView::title() const
{
    if(delayLoading)
    {
        return delayTitle;
    }
    else
    {
        return documentTitle();
    }
}

QPoint BrowserView::scrollPosition()
{
    QPoint value;
    if(delayScrolling)
    {
        value.setX(delayScrollX);
        value.setY(delayScrollY);
        return value;
    }

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

void BrowserView::setScrollPosition(int x, int y)
{
    QScrollBar* h = horizontalScrollBar();
    QScrollBar* v = verticalScrollBar();

    if(h != nullptr)
    {
        h->setValue(x);
    }

    if(v != nullptr)
    {
        v->setValue(y);
    }
}

QIcon BrowserView::icon()
{
    if(siteIcon.isNull())
    {
        siteIcon = QIcon(":/img/page.svg");
    }

    return siteIcon;
}

void BrowserView::setIcon(QString fileName)
{
    QIcon icon(fileName);
    iconFileName = fileName;

    if(icon.isNull())
    {
        siteIcon = QIcon(":/img/page.svg");
    }
    else
    {
        siteIcon = icon;
    }
    emit iconChanged(siteIcon);
}

void BrowserView::load()
{
    if(delayLoading)
    {
        delayLoading = false;
        navigateTo(delayUrl);
        if(delayScrolling)
        {
            qApp->processEvents();

            delayScrolling = false;
            setScrollPosition(delayScrollX, delayScrollY);
        }
    }
}

void BrowserView::delayLoad(QString theUrl, QString theTitle, int scrollX, int scrollY)
{
    delayUrl = theUrl;
    delayTitle = theTitle;
    delayScrollX = scrollX;
    delayScrollY = scrollY;

    delayLoading = true;
    delayScrolling = true;
}

void BrowserView::delayScroll(int scrollX, int scrollY)
{
    delayScrollX = scrollX;
    delayScrollY = scrollY;

    delayScrolling = true;
}

void BrowserView::setProgress(int value)
{
    if(0 <= value && value < 100)
    {
        if(progress == nullptr)
        {
            progress = new QProgressBar(this);
            resizeEvent(nullptr);
            progress->setVisible(true);
        }
        progress->setValue(value);
    }
    else
    {
        progress->setVisible(false);
    }

}

void BrowserView::setStatus(QString text)
{
    if(text != status->text())
    {
        if(text.isEmpty())
        {
            status->setVisible(false);
            status->clear();
            return;
        }

        status->setText(text);
        QFont font = status->font();
        font = QFont("Roboto", 13);

        QFontMetrics fm(font);
        QRect rect = fm.boundingRect(text);
        int maxWidth = width() - verticalScrollBar()->width();
        int lineHeight = rect.height();
        if(rect.width() > maxWidth)
        {
            rect.setHeight(lineHeight * (rect.width() / maxWidth + 1));
            rect.setWidth(maxWidth);
            rect = fm.boundingRect(rect, Qt::TextWordWrap, text);
            status->setWordWrap(true);
        }
        else
        {
            status->setWordWrap(false);
        }
        int pad = 14;
        int y = height() - rect.height();
        status->setGeometry(0, y, rect.width() + pad, rect.height());
        if(progress != nullptr)
        {
            progress->setGeometry(width() - 100, height() - PROGRESSBAR_HEIGHT, 100, PROGRESSBAR_HEIGHT);
        }
        status->setVisible(true);
    }
}

void BrowserView::navigateTo(QString text, bool changeHistory, bool feelingLucky)
{
    if(text.isEmpty())
    {
        about();
    }
    else
    {
        if(changeHistory && !text.startsWith("install:") && !text.startsWith("uninstall:") && !text.startsWith("unmask:"))
        {
            int scrollX = 0, scrollY = 0;
            QScrollBar* sb = horizontalScrollBar();
            if(sb != nullptr)
            {
                scrollX = sb->value();
            }

            sb = verticalScrollBar();
            if(sb != nullptr)
            {
                scrollY = sb->value();
            }

            appendHistory(text, scrollX, scrollY);
            emit urlChanged(text);
        }

        if(text.contains(':'))
        {
            setUrl(text);
        }
        else
        {
            searchApps(text, feelingLucky);
        }

        if(delayScrolling)
        {
            delayScrolling = false;
            setScrollPosition(delayScrollX, delayScrollY);
        }

    }
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
        QString cmd = "sudo emerge =" + app + " --verbose --verbose-conflicts --nospinner";
        if(window->ask)
        {
            cmd.append(" --ask");
        }
        /*
	QString logFile = app.mid(app.indexOf('/') + 1);
        logFile.prepend(ds->storageFolder);
        cmd.append(QString(" 2>&1 | tee %1.log").arg(logFile));
*/
        window->exec(cmd);
        window->install(app);
    }
    else if(scheme == "uninstall")
    {
        QString app = url.path(QUrl::FullyDecoded);
        QString cmd = "sudo emerge --unmerge =" + app + " --nospinner --noreplace";
        if(window->ask)
        {
            cmd.append(" --ask");
        }
        window->exec(cmd);
        window->uninstall(app);
    }
    else if(scheme == "unmask")
    {
        QString app = url.path(QUrl::FullyDecoded);
        QString cmd = "sudo emerge --autounmask =" + app + " --verbose --verbose-conflicts --nospinner";
        if(window->ask)
        {
            cmd.append(" --ask");
        }
        cmd.append(" && sudo dispatch-conf && sudo emerge =" + app + " --verbose --verbose-conflicts --nospinner");
        if(window->ask)
        {
            cmd.append(" --ask");
        }
        window->exec(cmd);
    }
    else if(scheme == "about")
    {
        about();
    }
    else if(scheme == "app")
    {
        viewApp(url);
    }
    else if(scheme == "file")
    {
        viewFile(url.toLocalFile());
    }
    else if(scheme == "new")
    {
        whatsNew("");
    }
}

void BrowserView::back()
{
    qDebug() << "history:";
    QString s;

    History item;
    for(int i = 0; i < history.count(); i++)
    {
        item = history.at(i);
        if(i == currentHistory)
        {
            qDebug() << "  " << item.target << item.scrollX << item.scrollY << "<-- you are here";
        }
        else
        {
            qDebug() << "  " << item.target << item.scrollX << item.scrollY;
        }
    }

    if(currentHistory > 0)
    {
        currentHistory--;
        History item = history.at(currentHistory);
        delayScroll(item.scrollX, item.scrollY);
        navigateTo(item.target, false);
        emit urlChanged(item.target);
    }
}

void BrowserView::forward()
{
    if(currentHistory < (history.count() - 1))
    {
        currentHistory++;
        History item = history.at(currentHistory);
        delayScroll(item.scrollX, item.scrollY);
        navigateTo(item.target, false);
        emit urlChanged(item.target);
    }
}

void BrowserView::reload(bool hardReload)
{
    clear();
    qApp->processEvents();

    if(history.count())
    {
        History item = history.at(currentHistory);
        delayScroll(item.scrollX, item.scrollY);
        if(hardReload && item.target.startsWith("app:"))
        {
            reloadApp(item.target);
        }

        navigateTo(item.target, false);
    }
    else
    {
        about();
    }
}

void BrowserView::stop()
{

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
    p.KEYWORDS, p.IUSE, c.CATEGORY, p.PACKAGE, p.MASKED, p.DOWNLOADSIZE
from PACKAGE p
inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID
inner join REPO r on r.REPOID = p.REPOID
where c.CATEGORY=? and p.PACKAGE=?
order by p.PACKAGE, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc
)EOF");

    QString search = url.path(QUrl::FullyDecoded);
    QStringList x = search.split('/');
    query.bindValue(0, x.first());
    query.bindValue(1, x.last());

    QString html;
    bool installed;
    bool obsoleted;
    bool masked;
    bool hasIcon = false;
    QString repo;
    QString slot;
    QString category;
    QString package;
    QString description;
    QString version;
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
    int firstUnmasked = -1;

    QHash<QString, QString> installedVersions;
    QString versionSize;

    QFile input;

    html = "<HTML>\n";
    html.append(QString("<HEAD><TITLE>%1</TITLE></HEAD>\n<BODY>").arg(search));

    isWorld = false;
    if(query.exec() && query.first())
    {
        while(1)
        {
            installed = query.value(4).toBool();
            if(installed == false)
            {
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
                continue;
            }

            //determine icon file
            category = query.value(11).toString();
            package = query.value(12).toString();
            version = query.value(2).toString();
            iuse = query.value(10).toString();
            appicon = QString("/var/db/pkg/%1/%2-%3/CONTENTS").arg(category).arg(package).arg(version);
            QStringList bigIcons;
            QStringList smallIcons;
            QStringList desktopFiles;
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
                    setIcon(smallIcons.first());
                    hasIcon = true;
                }
            }

            s = QString("/var/db/pkg/%1/%2-%3/USE").arg(category).arg(package).arg(version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                useFlags = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/IUSE").arg(category).arg(package).arg(version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                iuse = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/CFLAGS").arg(category).arg(package).arg(version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                cFlags = input.readAll();
                input.close();
            }

            s = QString("/var/db/pkg/%1/%2-%3/CXXFLAGS").arg(category).arg(package).arg(version);
            input.setFileName(s);
            if(input.open(QIODevice::ReadOnly))
            {
                cxxFlags = input.readAll();
                input.close();
            }
            break;
        }

        s = QString("/var/lib/portage/world");
        input.setFileName(s);
        if(input.open(QIODevice::ReadOnly))
        {
            QString worldSet = input.readAll();
            input.close();
            if(worldSet.contains(search))
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
                setIcon(iconMap[cat]);
            }
            else
            {
                qDebug() << "Couldn't find" << cat << "in icon map.";
                setIcon(":/img/page.svg");
            }
        }

        if(appicon.isEmpty())
        {
            html.append(QString(R"EOF(<P><B>%1</B><BR>%2</P><P>%3</P>)EOF").arg(search).arg(description).arg(homePage));
        }
        else
        {
            html.append(QString(R"EOF(
<TABLE>
<TR><TD><IMG SRC="%4"></TD>
<TD><P><BR><B>&nbsp;%1</B></P><P>&nbsp;%2</P><P>&nbsp;%3</P></TD></TR>
</TABLE>
)EOF").arg(search).arg(description).arg(homePage).arg(appicon));
        }

        html.append("<P><TABLE BORDER=0 CLASS=\"normal\">");
        html.append("<TR><TD><B>Repository&nbsp;&nbsp;</B></TD><TD><B>Slot&nbsp;&nbsp;</B></TD><TD><B>Version&nbsp;&nbsp;</B></TD><TD><B>Status</B>");
        if(isWorld)
        {
            html.append(" (@world member)");
        }
        html.append("</TD></TR>\n<TR><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD></TR>");
        query.first();
        do
        {
            repo = query.value(0).toString();
            version = query.value(2).toString();
            installed = query.value(4).toBool();
            obsoleted = query.value(5).toBool();
            masked = query.value(13).toBool();
            slot = query.value(6).toString();
            downloadSize = query.value(14).toInt();
            if(downloadSize != -1)
            {
                if(downloadSize > 1024 * 1024 * 1024)
                {
                    float size = static_cast<float>(downloadSize) / (1024.0f * 1024.0f * 1024.0f);
                    installedSize = QString(", %1 GB").arg(QString::number(size, 'f', 2));
                }
                else if(downloadSize > 1024 * 1024)
                {
                    float size = static_cast<float>(downloadSize) / (1024.0f * 1024.0f);
                    installedSize = QString(", %1 MB").arg(QString::number(size, 'f', 1));
                }
                else if(downloadSize > 1024)
                {
                    float size = static_cast<float>(downloadSize) / (1024.0f);
                    installedSize = QString(", %1 KB").arg(QString::number(size, 'f', 1));
                }
                else
                {
                    installedSize = QString(", %1 bytes").arg(downloadSize);
                }
            }
            else
            {
                installedSize.clear();
            }

            if(installed)
            {
                ebuild = QString("/var/db/pkg/%1/%2-%3/%2-%3.ebuild").arg(category).arg(package).arg(version);
            }
            else
            {
                ebuild = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(repo).arg(category).arg(package).arg(version);
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
                    s = "-";
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

            versionSize = QString("<A HREF=\"%1:%2/%3-%4\">%4</A>").arg(action).arg(category).arg(package).arg(version);
            html.append("<TR>");
            html.append(QString("<TD><A HREF=\"file://%2\">%1</A>&nbsp;&nbsp;</TD>").arg(repo).arg(ebuild));
            html.append(QString("<TD>%2&nbsp;&nbsp;</TD>").arg(slot));
            html.append(QString("<TD>%9&nbsp;&nbsp;</TD>").arg(versionSize));
            html.append(QString("<TD>%4</TD>").arg(s));
            html.append("</TR>\n");
        } while(query.next());
        html.append("</TABLE></P>\n");

        if(license.isEmpty() == false)
        {
            html.append(QString("<P><B>License:</B> %1</P>").arg(license));
        }

        if(useFlags.isEmpty() == false)
        {
            html.append(QString("<P><B>USE:</B> %1</P>").arg(useFlags));
        }

        if(iuse.isEmpty() == false)
        {
            html.append(QString("<P><B>IUSE:</B> %1</P>").arg(iuse));
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
    }

    html.append("</BODY>\n<HTML>\n");
    QString oldTitle = documentTitle();
    setText(html);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
    setFocus();
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
    QString categoryPath;
    QString buildsPath;
    QString ebuildFilePath;
    QString data;
    QString installedFilePath;
    QString repoFilePath;
    QString repo;
    qint64 installed;
    qint64 published;
    QFileInfo fi;
    int downloadSize = -1;
    QDir dir;
    QDir builds;
    QFile input;
    bool ok;

    QString sql = QString(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6, SLOT,
    LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED
)
values
(
    (select CATEGORYID from CATEGORY where CATEGORY=?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, 0, ?, ?, ?, 0, ?
)
)EOF");
    query.prepare(sql);
    for(int repoId = 0; repoId < portage->repos.count(); repoId++)
    {
        categoryPath = portage->repos.at(repoId);
        qDebug() << "Scanning repo:" << categoryPath;

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
            installedFilePath = QString("/var/db/pkg/%1/%2-%3").arg(category).arg(packageName).arg(portage->version.pvr);
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
            query.bindValue(12, portage->var("SLOT"));
            query.bindValue(13, portage->var("LICENSE"));
            query.bindValue(14, installed);
            query.bindValue(15, downloadSize);
            query.bindValue(16, portage->var("KEYWORDS"));
            query.bindValue(17, portage->var("IUSE"));
            query.bindValue(18, published);
            if(query.exec() == false)
            {
                qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                db.rollback();
                return;
            }
        }
    }

    bool obsolete;
    int repoId = 0;
    query.prepare(R"EOF(
insert into PACKAGE
(
    CATEGORYID, REPOID, PACKAGE, DESCRIPTION, HOMEPAGE, VERSION, V1, V2, V3, V4, V5, V6,
    SLOT, LICENSE, INSTALLED, OBSOLETED, DOWNLOADSIZE, KEYWORDS, IUSE, MASKED, PUBLISHED
)
values
(
    (select CATEGORYID from CATEGORY where CATEGORY=?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
    ?, ?, ?, ?, ?, ?, ?, 0, ?
)
)EOF");

    QRegularExpressionMatch match;
    QRegularExpression pvsplit;
    pvsplit.setPattern("(.+)-([0-9][0-9,\\-,\\.,[A-z]*)");
    QString package;
    QStringList packageNameFilter;
    packageNameFilter << QString("%1-*").arg(packageName);

    qDebug() << "Scanning installed packages...";
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

            installedFilePath = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(data).arg(category).arg(packageName).arg(portage->version.pvr);
            if(QFile::exists(installedFilePath))
            {
                // already imported this package from the repo directory
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

            ebuildFilePath = QString("%1/%2.ebuild").arg(buildsPath).arg(package);
            fi.setFile(ebuildFilePath);
            installed = fi.birthTime().toSecsSinceEpoch();
            published = 0;
            obsolete = true;

            portage->ebuildReader(ebuildFilePath);

            query.bindValue(0, category);
            if(repoId == -1)
            {
                query.bindValue(1, QVariant(QVariant::Int)); // NULL repoId
            }
            else
            {
                query.bindValue(1, repoId);
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
            query.bindValue(12, portage->var("SLOT"));
            query.bindValue(13, portage->var("LICENSE"));
            query.bindValue(14, installed);
            query.bindValue(15, obsolete);
            query.bindValue(16, downloadSize);
            query.bindValue(17, portage->var("KEYWORDS"));
            query.bindValue(18, portage->var("IUSE"));
            query.bindValue(19, published);
            if(query.exec() == false)
            {
                qDebug() << "Query failed:" << query.executedQuery() << query.lastError().text();
                db.rollback();
                return;
            }
        }
    }

    db.commit();
    portage->applyMasks(db);
    qDebug() << "Reload" << search << "completed.";
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

    setIcon(":/img/search.svg");// ":/img/search.svg"
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
    QString glob = search.replace('*', "%");
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

    setIcon(":/img/new.svg");
    showQueryResult(&query, result, search, feelingLucky);
}


void BrowserView::mousePressEvent(QMouseEvent* event)
{
    QString link;
    link = anchorAt(event->pos());

    if(link.isEmpty())
    {
        QTextEdit::mousePressEvent(event);
    }
}

void BrowserView::mouseMoveEvent(QMouseEvent* event)
{
    static QString oldLink;

    QString link;
    link = anchorAt(event->pos());

    if(link.isEmpty())
    {
        if(oldLink.isEmpty() == false)
        {
            setStatus("");
            oldLink.clear();
            viewport()->unsetCursor();
        }
    }
    else if(link != oldLink)
    {
        setStatus(link);
        oldLink = link;
        viewport()->setCursor(Qt::PointingHandCursor);
    }

    QTextEdit::mouseMoveEvent(event);
}

void BrowserView::mouseReleaseEvent(QMouseEvent* event)
{
    QString link;

    switch(event->button())
    {
        case Qt::BackButton:
            back();
            event->accept();
            return;

        case Qt::ForwardButton:
            forward();
            event->accept();
            return;

        case Qt::LeftButton:
            link = anchorAt(event->pos());
            if(link.isEmpty() || textCursor().hasSelection())
            {
                QTextEdit::mouseReleaseEvent(event);
                return;
            }

            navigateTo(link);
            event->accept();
            return;

        case Qt::MiddleButton:
            link = anchorAt(event->pos());
            if(link.isEmpty() || textCursor().hasSelection())
            {
                QTextEdit::mouseReleaseEvent(event);
                return;
            }

            emit openInNewTab(link);
            event->accept();
            return;

        default:
            QTextEdit::mouseReleaseEvent(event);
            return;
    }
}

void BrowserView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = nullptr;
    QAction* action;

    context = anchorAt(event->pos());
    if(context.isEmpty() == false)
    {
        menu = new QMenu("Browser Menu", this);

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
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);

            action = new QAction("Open link in new tab", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                BrowserView* view = window->tabWidget()->createTab();
                view->navigateTo(context);
            });
            menu->addAction(action);

            action = new QAction("Open link in new window", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                BrowserWindow* newWindow = window->openWindow();
                newWindow->currentView()->navigateTo(context);
            });
            menu->addAction(action);

            action = new QAction("Browse Folder", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                shell->externalFileManager(context);
            });
            menu->addAction(action);
        }
        else if(context.startsWith("uninstall:"))
        {
            action = new QAction("Copy atom to clipboard", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);

            action = new QAction("Verify integrity", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                shell->externalTerm(QString("qcheck =%1").arg(urlPath));
            });
            menu->addAction(action);

            action = new QAction("List files owned", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                shell->externalTerm(QString("qlist =%1 | less").arg(urlPath));
            });
            menu->addAction(action);

            action = new QAction("Who depends on this?", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                shell->externalTerm(QString("equery depends =%1").arg(urlPath));
            });
            menu->addAction(action);

            action = new QAction("Dependencies graph", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("equery depgraph =%1  | less").arg(app));
            });
            menu->addAction(action);

            action = new QAction("View size", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("qsize =%1").arg(app));
            });
            menu->addAction(action);

            action = new QAction("Reinstall", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo emerge =" + urlPath + " --verbose --verbose-conflicts --nospinner --ask";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Reinstall from source", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner --ask";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);
        }
        else if(context.startsWith("install:"))
        {
            action = new QAction("Copy atom to clipboard", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);

            action = new QAction("Install from source", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner --ask";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Fetch", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo emerge =" + urlPath + " --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Fetch source", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);
        }
        else if(context.startsWith("app:"))
        {
            action = new QAction("Copy atom to clipboard", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(urlPath);
            });
            menu->addAction(action);

            action = new QAction("Open link in new tab", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                BrowserView* view = window->tabWidget()->createTab();
                view->navigateTo(context);
            });
            menu->addAction(action);

            action = new QAction("Open link in new window", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                BrowserWindow* newWindow = window->openWindow();
                newWindow->currentView()->navigateTo(context);
            });
            menu->addAction(action);

            action = new QAction("Install", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo emerge =" + urlPath + " --verbose --verbose-conflicts --nospinner --ask";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Install from source", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --verbose --verbose-conflicts --nospinner --ask";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Fetch", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo emerge =" + urlPath + " --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);

            action = new QAction("Fetch source", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo FEATURES=\"${FEATURES} -getbinpkg\" emerge =" + urlPath + " --usepkg=n --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner";
                shell->externalTerm(cmd);
            });
            menu->addAction(action);
        }
        else
        {
            delete menu;
            menu = nullptr;
        }
    }
    else if(url().startsWith("app:"))
    {
        QString urlPath = url().mid(4);

        menu = new QMenu("App Menu", this);
        action = new QAction("Copy atom to clipboard", this);
        connect(action, &QAction::triggered, this, [this, urlPath]()
        {
            QClipboard* clip = qApp->clipboard();
            clip->setText(urlPath);
        });
        menu->addAction(action);

        if(isWorld == false)
        {
            action = new QAction("Add to @world set", this);
            connect(action, &QAction::triggered, this, [this, urlPath]()
            {
                QString cmd = "sudo emerge --noreplace " + urlPath + " --verbose --verbose-conflicts --nospinner --ask=n";
                shell->externalTerm(cmd);
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
                shell->externalTerm(cmd);
                isWorld = false;
            });
            menu->addAction(action);
        }
    }

    if(menu != nullptr)
    {
        menu->popup(event->globalPos());
    }
}

void BrowserView::resizeEvent(QResizeEvent* event)
{
    QTextEdit::resizeEvent(event);

    QFont font = status->font();
    font = QFont("Roboto", 13);

    QFontMetrics fm(font);
    QRect rect = fm.boundingRect(status->text());
    int maxWidth = width() - verticalScrollBar()->width();
    int lineHeight = rect.height();
    if(rect.width() > maxWidth)
    {
        //rect.setHeight(lineHeight * 4);
        rect.setHeight(lineHeight * (rect.width() / maxWidth + 1));
        rect.setWidth(maxWidth);
        rect = fm.boundingRect(rect, Qt::TextWordWrap, status->text());
        status->setWordWrap(true);
    }
    else
    {
        status->setWordWrap(false);
    }
    int pad = 14;
    int y = height() - rect.height();
    status->setGeometry(0, y, rect.width() + pad, rect.height());
    if(progress != nullptr)
    {
        progress->setGeometry(width() - 100, height() - PROGRESSBAR_HEIGHT, 100, PROGRESSBAR_HEIGHT);
    }
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
                app = QString("%1/%2").arg(category).arg(package);
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

                appCount++;
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
        History item;
        item.target = QString("app:%1/%2").arg(bestCategory).arg(bestPackage);
        item.scrollX = 0;
        item.scrollY = 0;
        history[currentHistory] = item;
        emit urlChanged(item.target);
        viewApp(item.target);
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
        result.append(QString("<P><B><A HREF=\"app:%1\">%1-%2</A> (").arg(app).arg(latestVersion));

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
                    result.append(QString("installed %1, obsolete %2)</B><BR>").arg(installed).arg(obsoleted));
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
                    result.append(QString("installed %1, obsolete %2)</B><BR>").arg(installed).arg(obsoleted));
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

void BrowserView::about()
{
    setIcon(":/img/appicon.svg");

    QString text = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>About</TITLE>
<HEAD>
<BODY>
<IMG SRC=":/img/appicon.svg" HEIGHT=238 ALIGN=RIGHT><P><FONT SIZE=+2>%1</FONT><BR>
%2</P>

<P>This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.</P>

<P>This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.</P>

<P>You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.</P>

</BODY>
</HTML>
)EOF").arg(APP_NAME).arg(APP_HTMLCOPYRIGHT);
    QString oldTitle = documentTitle();
    setText(text);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
    }
    setFocus();
}

void BrowserView::reloadingDatabase()
{
    setIcon(":/img/appicon.svg");

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

    if(!input.open(QIODevice::ReadOnly))
    {
        error("File '" + fileName + "' could not be opened.");
        return;
    }

    setIcon(":/img/page.svg");
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
        setText(data);
    }

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

    window->focusLineEdit();
}

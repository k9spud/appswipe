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

BrowserView::BrowserView(QWidget *parent) : QTextEdit(parent)
{
    status = new QLabel(this);
    status->setStyleSheet(R"EOF(
padding-top: 6px;
padding-bottom: 4px;
padding-left: 5px;
padding-right: 5px;
background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                stop: 0 rgba(21, 21, 13, 0), stop: 0.25 rgba(21, 21, 13, 255),
                                stop: 1.0 rgba(21, 21, 13, 255));
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

    iconMap["app"] = ":/img/app.svg";
    iconMap["dev"] = ":/img/dev.svg";
    iconMap["games"] = ":/img/games.svg";
    iconMap["gnustep"] = ":/img/gnustep.svg";
    iconMap["gui"] = ":/img/x11.svg";
    //iconMap["mail"] = ":img/.svg";
    iconMap["media"] = ":/img/media.svg";
    iconMap["net"] = ":/img/net.svg";
    iconMap["sci"] = ":/img/sci.svg";
    iconMap["sys"] = ":/img/sys.svg";
    iconMap["www"] = ":/img/www.svg";
    iconMap["x11"] = ":/img/x11.svg";
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

void BrowserView::appendHistory(QString text)
{
    while(currentHistory < history.count() - 1)
    {
        history.removeLast();
    }

    history.append(text);
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
        url = history.at(currentHistory);
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
        qDebug() << "Icon is null:" << fileName;
    }

    siteIcon = icon;
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
        int maxWidth = width();
        int lineHeight = rect.height();
        if(rect.width() > maxWidth)
        {
            rect.setHeight(lineHeight * 4);
            rect.setWidth(maxWidth);
            rect = fm.boundingRect(rect, Qt::TextWordWrap, text);
            status->setWordWrap(true);
        }
        else
        {
            status->setWordWrap(false);
        }
        int pad = 10;
        int y = height() - (rect.height() + pad);
        status->setGeometry(0, y, rect.width() + (pad * 2), rect.height() + pad);
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
            appendHistory(text);
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

        emit urlChanged(text);
        setFocus();
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
        QFile input(url.toLocalFile());
        if(!input.exists())
        {
            error("File '" + input.fileName() + "' does not exist.");
            return;
        }

        if(!input.open(QIODevice::ReadOnly))
        {
            error("File '" + input.fileName() + "' could not be opened.");
            return;
        }

        setIcon(":/img/page.svg");
        QByteArray data = input.readAll();
        QString oldTitle = documentTitle();
        setText(data);

        if(documentTitle() != oldTitle)
        {
            emit titleChanged(documentTitle());
        }
    }
}

void BrowserView::back()
{
    qDebug() << "history:";
    QString s;

    for(int i = 0; i < history.count(); i++)
    {
        s = history.at(i);
        if(i == currentHistory)
        {
            qDebug() << "  " << s << "<-- you are here";
        }
        else
        {
            qDebug() << "  " << s;
        }
    }

    if(currentHistory > 0)
    {
        currentHistory--;
        navigateTo(history.at(currentHistory), false);
    }
}

void BrowserView::forward()
{
    if(currentHistory < (history.count() - 1))
    {
        currentHistory++;
        navigateTo(history.at(currentHistory), false);
    }
}

void BrowserView::reload()
{
    clear();
    qApp->processEvents();

    if(history.count())
    {
        QString text = history.at(currentHistory);
        if(text.startsWith("app:"))
        {
            //qDebug() << "Reloading:" << text;
            reloadApp(text);
        }

        navigateTo(text, false);
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
    query.prepare("select r.REPO, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.OBSOLETED, p.SLOT, p.HOMEPAGE, p.LICENSE, P.KEYWORDS, p.IUSE, c.CATEGORY, p.PACKAGE, p.MASKED, p.DOWNLOADSIZE from PACKAGE p inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID inner join REPO r on r.REPOID = p.REPOID where c.CATEGORY=? and p.PACKAGE=? order by p.PACKAGE, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc");
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
    QString arch = "arm64";
    QString ebuild;
    QString appicon;
    QString action;
    QString s;
    QString iuse;
    QHash<QString, QString> installedVersions;
    QString versionSize;

    QFile input;

    html = "<HTML>\n";
    html.append(QString("<HEAD><TITLE>%1</TITLE></HEAD>\n<BODY>").arg(search));

    if(query.exec() && query.first())
    {
        while(1)
        {
            installed = query.value(4).toBool();
            if(installed)
            {
                //determine icon file
                category = query.value(11).toString();
                package = query.value(12).toString();
                version = query.value(2).toString();
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
                break;
            }
            else if(query.next() == false)
            {
                break;
            }
        }
        query.first();

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
        iuse = query.value(10).toString();
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
        html.append("<TR><TD><B>Repository&nbsp;&nbsp;</B></TD><TD><B>Slot&nbsp;&nbsp;</B></TD><TD><B>Version&nbsp;&nbsp;</B></TD><TD><B>Status</B></TD></TR>\n");
        html.append("<TR><TD><HR></TD><TD><HR></TD><TD><HR></TD><TD><HR></TD></TR>");
        do
        {
            repo = query.value(0).toString();
            version = query.value(2).toString();
            installed = query.value(4).toBool();
            obsoleted = query.value(5).toBool();
            masked = query.value(13).toBool();
            slot = query.value(6).toString();
            downloadSize = query.value(14).toInt();

            if(installed)
            {
                ebuild = QString("/var/db/pkg/%1/%2-%3/%2-%3.ebuild").arg(category).arg(package).arg(version);
            }
            else
            {
                ebuild = QString("/var/db/repos/%1/%2/%3/%3-%4.ebuild").arg(repo).arg(category).arg(package).arg(version);
            }

            s = query.value(9).toString();
            if(!s.contains(arch))
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
                if(masked)
                {
                    s = "masked";
                }
                else
                {
                    int i = s.indexOf(arch);
                    if(i == 0 || s.at(i - 1) == ' ')
                    {
                        s = "stable";
                    }
                    else if(i > 0 && s.at(i-1) == '~')
                    {
                        s = "testing";
                    }
                }
            }

            s = s + (obsoleted ? " (obsolete)" : installed ? " (installed)" : "");
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
            if(downloadSize != -1)
            {
                versionSize.append(" (");
                if(downloadSize > 1024 * 1024 * 1024)
                {
                    float size = static_cast<float>(downloadSize) / (1024.0f * 1024.0f * 1024.0f);
                    versionSize.append(QString::number(size, 'f', 1) + "GB");
                    versionSize.append(QString::number(downloadSize / (1024*1024*1024)) + "GB");
                }
                else if(downloadSize > 1024 * 1024)
                {
                    float size = static_cast<float>(downloadSize) / (1024.0f * 1024.0f);
                    versionSize.append(QString::number(size, 'f', 1) + "MB");
                }
                else if(downloadSize > 1024)
                {
                    versionSize.append(QString::number(downloadSize / (1024)) + "KB");
                }
                else
                {
                    versionSize.append(QString::number(downloadSize));
                }
                versionSize.append(")");
            }

            html.append("<TR>");
            html.append(QString("<TD><A HREF=\"file://%2\">%1</A>&nbsp;&nbsp;</TD>").arg(repo).arg(ebuild));
            html.append(QString("<TD>%2&nbsp;&nbsp;</TD>").arg(slot));
            html.append(QString("<TD>%9&nbsp;&nbsp;</TD>").arg(versionSize));
            html.append(QString("<TD>%4</TD>").arg(s));
            html.append("</TR>\n");
        } while(query.next());
        html.append("</TABLE></P>\n");

        html.append(QString("<P><B>License:</B> %1</P>").arg(license));
        html.append(QString("<P><B>IUSE:</B> %1</P>").arg(iuse));
        html.append(QString("<P><B>Keywords:</B> %1</P>").arg(keywords));
    }

    html.append("</BODY>\n<HTML>\n");
    QString oldTitle = documentTitle();
    setText(html);

    if(documentTitle() != oldTitle)
    {
        emit titleChanged(documentTitle());
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
    QString reloadPackage = x.last();
    QString categoryPath;
    QString buildsPath;
    QString packageName;
    QString ebuildFilePath;
    QString data;
    QString installedFilePath;
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
        qDebug() << "Scanning repo:" << portage->repos.at(repoId);

        categoryPath = portage->repos.at(repoId);
        categoryPath.append(category);
        dir.setPath(categoryPath);
        foreach(packageName, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
        {
            if(packageName != reloadPackage)
            {
                continue;
            }
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

                fi.setFile(buildsPath + "/" + ebuildFilePath);
                published = fi.birthTime().toSecsSinceEpoch();

                input.setFileName(buildsPath + "/" + ebuildFilePath);
                if(!input.open(QIODevice::ReadOnly))
                {
                    qDebug() << "Can't open ebuild:" << input.fileName();
                }
                else
                {
                    data = input.readAll();
                    input.close();
                    portage->ebuildParser(data);
                }

                query.bindValue(0, category);
                query.bindValue(1, repoId);
                query.bindValue(2, packageName);
                query.bindValue(3, portage->var("DESCRIPTION"));
                query.bindValue(4, portage->var("HOMEPAGE"));
                query.bindValue(5, portage->version.pvr);
                query.bindValue(6, portage->version.cut(0));
                query.bindValue(7, portage->version.cut(1));
                query.bindValue(8, portage->version.cut(2));
                query.bindValue(9, portage->version.cut(3));
                query.bindValue(10, portage->version.cut(4));
                query.bindValue(11, portage->var("PR"));
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
    }

    QString repoFilePath;
    bool obsolete;
    int repoId = 0;
    QString repo;
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

    qDebug() << "Scanning installed packages...";
    categoryPath = QString("/var/db/pkg/%1/").arg(category);
    dir.setPath(categoryPath);
    foreach(package, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot))
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

        if(packageName != reloadPackage)
        {
            continue;
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

            input.setFileName(ebuildFilePath);
            if(!input.open(QIODevice::ReadOnly))
            {
                qDebug() << "Can't open ebuild:" << input.fileName();
            }
            else
            {
                data = input.readAll();
                input.close();
                portage->ebuildParser(data);
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
            query.bindValue(2, packageName);
            query.bindValue(3, portage->var("DESCRIPTION"));
            query.bindValue(4, portage->var("HOMEPAGE"));
            query.bindValue(5, portage->version.pvr);
            query.bindValue(6, portage->version.cut(0));
            query.bindValue(7, portage->version.cut(1));
            query.bindValue(8, portage->version.cut(2));
            query.bindValue(9, portage->version.cut(3));
            query.bindValue(10, portage->version.cut(4));
            query.bindValue(11, portage->version.pr());
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
/*
    db.transaction();
    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%9999%'") == false)
    {
        db.rollback();
        return;
    }

    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%_alpha%'") == false)
    {
        db.rollback();
        return;
    }

    if(query.exec("update PACKAGE set MASKED=1 where VERSION like '%_beta%'") == false)
    {
        db.rollback();
        return;
    }
    db.commit();
*/
    qDebug() << "Reload" << search << "completed.";
}

void BrowserView::printApp(QString& result, QHash<QString, QString>& installedVersions, QHash<QString, QString>& obsoletedVersions, QStringList& apps, QSqlQuery& query)
{
    QString app = query.value(0).toString() + "/" + query.value(1).toString();
    QString version = query.value(2).toString();
    bool masked = query.value(5).toBool();

    if(apps.contains(app) || masked)
    {
        return;
    }

    apps.append(app);
    if(installedVersions[app].isEmpty() && obsoletedVersions[app].isEmpty())
    {
        // no versions of this app have been installed
        result.append(QString("<P><A HREF=\"app:%1/%2\">%1/%2</A><BR>").arg(query.value(0).toString()).arg(query.value(1).toString()));
    }
    else
    {
        result.append(QString("<P><B><A HREF=\"app:%1/%2\">%1/%2-%3</A> (").arg(query.value(0).toString()).arg(query.value(1).toString()).arg(query.value(2).toString()));

        if(installedVersions[app] == version)
        {
            if(obsoletedVersions[app].isEmpty())
            {
                // latest version of app is installed, no obsoleted versions.
                result.append(QString("installed)</B><BR>"));
            }
            else
            {
                // latest version of app is installed, along with one or more obsoleted versions.
                result.append(QString("installed, obsolete %1)</B><BR>").arg(obsoletedVersions[app].trimmed()));
            }
        }
        else
        {
            if(installedVersions[app].isEmpty())
            {
                // only obsoleted version(s) are installed
                result.append(QString("obsolete %1)</B><BR>").arg(obsoletedVersions[app].trimmed()));
            }
            else
            {
                if(obsoletedVersions[app].isEmpty())
                {
                    // no obsoleted versions installed
                    result.append(QString("installed %1)</B><BR>").arg(installedVersions[app].trimmed()));
                }
                else
                {
                    // both supported and obsoleted versions installed
                    result.append(QString("installed %1, obsolete %2)</B><BR>").arg(installedVersions[app].trimmed()).arg(obsoletedVersions[app].trimmed()));
                }
            }
        }
    }

    QString description = query.value(3).toString();
    if(description.isEmpty())
    {
        description = "(no description available)";
    }
    result.append(QString("%1</P>").arg(description));
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

    QStringList apps;
    QString app;
    QString package;
    QString result;
    QString installedApps;
    QString availableApps;
    bool installed;
    bool obsoleted;
    bool tooManyResults = false;

    QVector<int> skipApps;
    QHash<QString, QString> installedVersions;
    QHash<QString, QString> obsoletedVersions;

    result = "<HTML>\n";
    result.append(QString("<HEAD><TITLE>%1 search</TITLE></HEAD>\n<BODY>").arg(search));
    QSqlQuery query(db);
    QString glob = search.replace('*', "%");
    if(search.contains('/'))
    {
        query.prepare("select c.CATEGORY, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED from PACKAGE p inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID where c.CATEGORY like ? and p.PACKAGE like ? order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc limit 10000");
        QStringList x = glob.split('/');
        query.bindValue(0, "%" + x.first() + "%");
        query.bindValue(1, "%" + x.last() + "%");
    }
    else
    {
        query.prepare("select c.CATEGORY, p.PACKAGE, p.VERSION, p.DESCRIPTION, p.INSTALLED, p.MASKED, p.OBSOLETED from PACKAGE p inner join CATEGORY c on c.CATEGORYID = p.CATEGORYID where p.PACKAGE like ? or p.DESCRIPTION like ? or c.CATEGORY=? order by c.CATEGORY, p.PACKAGE, p.MASKED, p.V1 desc, p.V2 desc, p.V3 desc, p.V4 desc, p.V5 desc, p.V6 desc limit 10000");
        query.bindValue(0, "%" + glob + "%");
        query.bindValue(1, "%" + glob + "%");
        query.bindValue(2, glob);
    }

    if(query.exec())
    {
        while(query.next())
        {
            package = query.value(1).toString();
            app = query.value(0).toString() + "/" + package;

            if(apps.contains(app) == false)
            {
                apps.append(app);

                if(search.contains('/') == false && package == search)
                {
                    skipApps.prepend(query.at());
                }
            }

            installed = query.value(4).toInt() != 0;
            obsoleted = query.value(6).toInt() != 0;
            if(obsoleted)
            {
                if(obsoletedVersions[app].isEmpty())
                {
                    obsoletedVersions[app] = query.value(2).toString();
                }
                else
                {
                    obsoletedVersions[app].append(", " + query.value(2).toString());
                }
            }
            else if(installed)
            {
                if(installedVersions[app].isEmpty())
                {
                    installedVersions[app] = query.value(2).toString();
                }
                else
                {
                    installedVersions[app].append(", " + query.value(2).toString());
                }
            }
        }

        if(apps.count() == 0)
        {
            error("No results found.");
            return;
        }
        else if(apps.count() == 1 || feelingLucky)
        {
            if(skipApps.count())
            {
                query.seek(skipApps.first());
            }
            else
            {
                query.first();
            }
            QString app = QString("app:%1/%2").arg(query.value(0).toString()).arg(query.value(1).toString());
            history[currentHistory] = app;
            emit urlChanged(app);
            viewApp(app);
            return;
        }
        else
        {
            query.last();
            if(query.at() >= 9999)
            {
                tooManyResults = true;
            }

            apps.clear();
            foreach(int i, skipApps)
            {
                query.seek(i);
                printApp(result, installedVersions, obsoletedVersions, apps, query);
            }

            query.first();
            QString app;
            do
            {
                app = query.value(0).toString() + "/" + query.value(1).toString();
                if(installedVersions[app].isEmpty() && obsoletedVersions[app].isEmpty())
                {
                    printApp(availableApps, installedVersions, obsoletedVersions, apps, query);
                }
                else
                {
                    printApp(installedApps, installedVersions, obsoletedVersions, apps, query);
                }
            } while(query.next());
        }
    }
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
    setIcon(":/img/search.svg");// ":/img/search.svg"
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

        QUrl url(context);
        QString urlPath = url.path(QUrl::FullyDecoded);

        if(context.startsWith("http"))
        {
            action = new QAction("Copy link address", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QClipboard* clip = qApp->clipboard();
                clip->setText(context);
            });
            menu->addAction(action);

            action = new QAction("Open link in browser", this);
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
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("qcheck =%1").arg(app));
            });
            menu->addAction(action);

            action = new QAction("List files owned", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("qlist =%1").arg(app));
            });
            menu->addAction(action);


            action = new QAction("Who depends on this?", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("equery depends =%1").arg(app));
            });
            menu->addAction(action);

            action = new QAction("Dependencies graph", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                shell->externalTerm(QString("equery depgraph =%1").arg(app));
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

            action = new QAction("Reinstall from source", this);
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                QString cmd = "sudo emerge =" + app + " --usepkg=n --verbose --verbose-conflicts --nospinner --ask";
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
            connect(action, &QAction::triggered, this, [this]()
            {
                QUrl url(context);
                QString app = url.path(QUrl::FullyDecoded);
                QString cmd = "sudo emerge =" + app + " --usepkg=n --verbose --verbose-conflicts --nospinner --ask";
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

    if(menu != nullptr)
    {
        menu->popup(event->globalPos());
    }
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
}

void BrowserView::reloadingDatabase()
{
    setIcon(":/img/appicon.svg");

    QString text = QString(R"EOF(
<HTML>
<HEAD>
<TITLE>Reloading Database</TITLE>
<HEAD>
<BODY>
<P ALIGN=CENTER><CENTER><IMG SRC=":/img/appicon.svg"></CENTER></P>
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
}

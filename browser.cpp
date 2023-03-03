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

#include "browser.h"
#include "browserwindow.h"
#include "tabwidget.h"
#include "compositeview.h"
#include "datastorage.h"
#include "globals.h"

#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QIcon>
#include <QDebug>
#include <QProcessEnvironment>

int sighupFd[2];
int sigtermFd[2];

Browser::Browser(QObject *parent) : QObject(parent)
{
    // initialize SIGHUP/SIGTERM signal handling
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd))
    {
       qDebug("Couldn't create SIGHUP socketpair");
    }
    else
    {
        sighupNotifier = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
        connect(sighupNotifier, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSIGHUP()));

        struct sigaction hup;

        hup.sa_handler = Browser::unixSIGHUP;
        sigemptyset(&hup.sa_mask);
        hup.sa_flags = 0;
        hup.sa_flags |= SA_RESTART;

        sigaction(SIGHUP, &hup, 0);
    }

    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
    {
       qDebug("Couldn't create SIGTERM socketpair");
    }
    else
    {
        sigtermNotifier = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
        connect(sigtermNotifier, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSIGTERM()));

        struct sigaction term;

        term.sa_handler = Browser::unixSIGTERM;
        sigemptyset(&term.sa_mask);
        term.sa_flags = 0;
        term.sa_flags |= SA_RESTART;

        sigaction(SIGTERM, &term, 0);
    }
}

void Browser::unixSIGHUP(int unused)
{
    Q_UNUSED(unused);
    char a = 1;
    ssize_t i = ::write(sighupFd[0], &a, sizeof(a));
    Q_UNUSED(i);
}

void Browser::unixSIGTERM(int unused)
{
    Q_UNUSED(unused);
    char a = 1;
    ssize_t i = ::write(sigtermFd[0], &a, sizeof(a));
    Q_UNUSED(i);
}

void Browser::handleSIGHUP()
{
    sighupNotifier->setEnabled(false);
    char tmp;
    ssize_t k = ::read(sighupFd[1], &tmp, sizeof(tmp));
    Q_UNUSED(k);

    CompositeView* v;
    TabWidget* tabs;
    foreach(BrowserWindow* w, windows)
    {
        tabs = w->tabWidget();
        for(int i = 0; i < tabs->count(); i++)
        {
            v = tabs->viewAt(i);
            if(v->delayLoading)
            {
                continue;
            }

            if(v->url().startsWith("app:") || v->url().startsWith("update:"))
            {
                v->reload(false);
            }
        }
    }

    sighupNotifier->setEnabled(true);
}

void Browser::handleSIGTERM()
{
    sigtermNotifier->setEnabled(false);
    char tmp;
    ssize_t i = ::read(sigtermFd[1], &tmp, sizeof(tmp));
    Q_UNUSED(i);

    saveAllWindows();
    sigtermNotifier->setEnabled(true);
}

Browser::~Browser()
{
    saveAllWindows();
    closeAll();

    QSqlDatabase::removeDatabase(ds->connectionName);
}

BrowserWindow* Browser::createWindow(int windowId)
{
    BrowserWindow* window = new BrowserWindow();
    if(windowId == -1)
    {
        window->windowId = createWindowId();
    }
    else
    {
        window->windowId = windowId;
    }
    window->setObjectName(QString("Window %1").arg(window->windowId + 1));
    windows.append(window);
    connect(window, &QObject::destroyed, this, [this, window]()
    {
        windows.removeOne(window);
    });
    return window;
}

void Browser::discardWindow(int windowId)
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery query(db);

    db.transaction();

    if(discardWindow(windowId, query) == false)
    {
        db.rollback();
        return;
    }

    db.commit();
    db.close();
}

bool Browser::discardWindow(int windowId, QSqlQuery& query)
{
    query.prepare("delete from WINDOW where WINDOWID = ?");
    query.bindValue(0, windowId);
    if(query.exec() == false)
    {
        qDebug() << "Could not delete " << windowId << " from WINDOW.";
        return false;
    }

    query.prepare("delete from TAB where WINDOWID = ?");
    query.bindValue(0, windowId);
    if(query.exec() == false)
    {
        qDebug() << "Could not delete " << windowId << " from TAB.";
        return false;
    }

    return true;
}

void Browser::closeAll()
{
    foreach(BrowserWindow* window, windows)
    {
        window->deleteLater();
    }
    windows.clear();
}

QVector<Browser::WindowHash> Browser::inactiveWindows()
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    QVector<WindowHash> windows;

    if(qry.exec("select WINDOWID, TITLE from WINDOW where STATUS=0 order by TITLE"))
    {
        WindowHash w;
        while(qry.next())
        {
            w.windowId = qry.value(0).toInt();
            w.title = qry.value(1).toString();
            windows.append(w);
        }
    }

    return windows;
}

void Browser::loadWindows(QSqlQuery& windowQuery, QSqlQuery& tabQuery)
{
    tabQuery.prepare("select URL, TITLE, CURRENTPAGE, SCROLLX, SCROLLY, ICON from TAB where WINDOWID=? order by TABID");

    if(windowQuery.exec())
    {
        BrowserWindow* window;
        CompositeView* view;
        QString url;
        QString title;
        QString icon;
        int scrollX, scrollY;
        int currentPage;
        int page;
        int x, y, w, h;
        int status;

        while(windowQuery.next())
        {
            window = createWindow(windowQuery.value(0).toInt());

            x = windowQuery.value(1).toInt();
            y = windowQuery.value(2).toInt();
            w = windowQuery.value(3).toInt();
            h = windowQuery.value(4).toInt();
            if(isWayland())
            {
                window->resize(w, h);
            }
            else
            {
                // for some reason, this messes up mapToGlobal() for popup menus on Wayland
                window->setGeometry(x, y, w, h);
            }

            title = windowQuery.value(5).toString();
            if(title.isEmpty())
            {
                title = QString("Window %1").arg(window->windowId + 1);
            }
            window->setObjectName(title);

            window->clip = (windowQuery.value(6).toInt() == 1);
            window->updateClipButton();

            window->ask = (windowQuery.value(7).isNull() || windowQuery.value(7).toInt() == 1);
            window->updateAskButton();
            status = windowQuery.value(8).toInt();

            tabQuery.bindValue(0, window->windowId);
            if(tabQuery.exec())
            {
                page = 0;
                currentPage = 0;
                while(tabQuery.next())
                {
                    url = tabQuery.value(0).toString();
                    title = tabQuery.value(1).toString();
                    scrollX = tabQuery.value(3).toInt();
                    scrollY = tabQuery.value(4).toInt();
                    icon = tabQuery.value(5).toString();
                    if(icon == ":/img/clipboard.svg")
                    {
                        QStringList list = url.split(' ');
                        if(list.first() == "install:")
                        {
                            for(int i = 1; i < list.count(); i++)
                            {
                                window->install(list.at(i), false);
                            }
                        }
                        else if(list.first() == "uninstall:")
                        {
                            for(int i = 1; i < list.count(); i++)
                            {
                                window->uninstall(list.at(i));
                            }
                        }
                        url.clear();
                        page++;

                        if(tabQuery.value(2).toInt() != 0) // is current page?
                        {
                            currentPage = page;
                            window->setWindowTitle(QString("%1 - %2").arg(title, window->objectName()));
                        }
                        continue;
                    }

                    if(page == 0)
                    {
                        view = window->currentView();
                    }
                    else
                    {
                        view = window->tabWidget()->createBackgroundTab();
                    }

                    if(tabQuery.value(2).toInt() != 0) // is current page?
                    {
                        currentPage = page;
                        if(url.isEmpty() == false || title.isEmpty() == false)
                        {
                            view->delayScroll(QPoint(scrollX, scrollY));
                            view->navigateTo(url);
                            view->setFocus();
                        }
                        window->setWindowTitle(QString("%1 - %2").arg(title, window->objectName()));
                    }
                    else
                    {
                        if(url.isEmpty() == false || title.isEmpty() == false)
                        {
                            view->delayLoad(url, title, scrollX, scrollY);
                        }
                    }

                    view->setIcon(icon);
                    page++;
                }
                window->tabWidget()->setCurrentIndex(currentPage);
                window->tabWidget()->insertAfter = currentPage;
            }
            if(status & WindowStatus::Maximized)
            {
                window->showMaximized();
            }
            else
            {
                window->show();
            }
        }
    }
}

void Browser::restoreWindows()
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery tabQuery(db);
    QSqlQuery windowQuery(db);

    windowQuery.prepare("select WINDOWID, X, Y, W, H, TITLE, CLIP, ASK, STATUS from WINDOW where STATUS is null or STATUS != 0 order by WINDOWID");
    loadWindows(windowQuery, tabQuery);
}

void Browser::loadWindow(int windowId)
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery tabQuery(db);
    QSqlQuery windowQuery(db);

    windowQuery.prepare("select WINDOWID, X, Y, W, H, TITLE, CLIP, ASK, STATUS from WINDOW where WINDOWID=?");
    windowQuery.bindValue(0, windowId);
    loadWindows(windowQuery, tabQuery);

    windowQuery.prepare("update WINDOW set STATUS=1 where WINDOWID = ?");
    windowQuery.bindValue(0, windowId);
    windowQuery.exec();
}

int Browser::createWindowId()
{
    int windowId = 0;

    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    if(qry.exec("select max(WINDOWID) from WINDOW"))
    {
        if(qry.next())
        {
            if(qry.value(0).isNull())
            {
                windowId = 0;
            }
            else
            {
                windowId = qry.value(0).toInt() + 1;
            }
        }
    }

    foreach(BrowserWindow* w, windows)
    {
        if(w->windowId >= windowId)
        {
            windowId = w->windowId + 1;
        }
    }

    return windowId;
}

void Browser::saveWindow(BrowserWindow* window, bool active, QSqlQuery& qwindow, QSqlQuery& query)
{
    int windowStatus = (active ? Browser::Active : Browser::Inactive);
    windowStatus |= (window->isMaximized() ? Browser::Maximized : 0);

    int tabId = 0;
    if(query.exec("select max(TABID) from TAB"))
    {
        if(query.next())
        {
            tabId = query.value(0).toInt() + 1;
        }
    }

    int tabCount;

    QRect r;
    if(isWayland())
    {
        r = window->frameGeometry();
    }
    else
    {
        r = window->geometry();
    }

    qwindow.prepare("insert into WINDOW (WINDOWID, X, Y, W, H, TITLE, STATUS, CLIP, ASK) values (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.prepare("insert into TAB (WINDOWID, TABID, URL, TITLE, SCROLLX, SCROLLY, CURRENTPAGE, ICON) values (?, ?, ?, ?, ?, ?, ?, ?)");

    qwindow.bindValue(0, window->windowId);
    qwindow.bindValue(1, r.x());
    qwindow.bindValue(2, r.y());
    qwindow.bindValue(3, window->unmaximizedSize.width());
    qwindow.bindValue(4, window->unmaximizedSize.height());
    qwindow.bindValue(5, window->objectName());
    qwindow.bindValue(6, windowStatus);
    qwindow.bindValue(7, window->clip);
    qwindow.bindValue(8, window->ask);

    if(qwindow.exec() == false)
    {
        qDebug() << "Query failed:" << qwindow.executedQuery();
    }

    CompositeView* view;
    tabCount = window->tabWidget()->count();
    for(int i = 0; i < tabCount; i++)
    {
        view = window->tabWidget()->viewAt(i);

        QPoint scroll = view->scrollPosition();
        query.bindValue(0, window->windowId);
        query.bindValue(1, tabId++);

        if(view == window->installView)
        {
            query.bindValue(2, "install: " + window->installList.join(' '));
        }
        else if(view == window->uninstallView)
        {
            query.bindValue(2, "uninstall: " + window->uninstallList.join(' '));
        }
        else
        {
            query.bindValue(2, view->url());
        }
        query.bindValue(3, view->title());
        query.bindValue(4, scroll.x());
        query.bindValue(5, scroll.y());
        query.bindValue(6, (i == window->tabWidget()->currentIndex()));
        query.bindValue(7, view->iconFileName);
        if(query.exec() == false)
        {
            qDebug() << "Query failed:" << query.executedQuery();
        }
    }
}

void Browser::saveWindow(BrowserWindow* window, bool active)
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery query(db);
    QSqlQuery qwindow(db);

    db.transaction();

    discardWindow(window->windowId, query);
    saveWindow(window, active, qwindow, query);

    db.commit();
    db.close();
}

void Browser::saveAllWindows()
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery query(db);
    QSqlQuery qwindow(db);

    db.transaction();
    foreach(BrowserWindow* window, windows)
    {
        discardWindow(window->windowId, query);
    }

    foreach(BrowserWindow* window, windows)
    {
        saveWindow(window, true, qwindow, query);
    }

    db.commit();
    db.close();
}

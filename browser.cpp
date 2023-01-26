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
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    QSqlQuery qwindow(db);
    qry.prepare("select URL, TITLE, CURRENTPAGE, SCROLLX, SCROLLY, ICON from TAB where WINDOWID=? order by TABID");

    int windowId;
    BrowserWindow* window;
    CompositeView* view;
    QString url;
    QString title;
    QString icon;
    int scrollX, scrollY;
    int currentPage;
    int page;

    int x, y, w, h;
    if(qwindow.exec("select WINDOWID, X, Y, W, H from WINDOW order by WINDOWID"))
    {
        while(qwindow.next())
        {
            windowId = qwindow.value(0).toInt();
            window = createWindow();
            x = qwindow.value(1).toInt();
            y = qwindow.value(2).toInt();
            w = qwindow.value(3).toInt();
            h = qwindow.value(4).toInt();

            if(isWayland())
            {
                window->resize(w, h);
            }
            else
            {
                // for some reason, this messes up mapToGlobal() for popup menus on Wayland
                window->setGeometry(x, y, w, h);
            }

            qry.bindValue(0, windowId);
            if(qry.exec())
            {
                page = 0;
                currentPage = 0;
                while(qry.next())
                {
                    url = qry.value(0).toString();
                    title = qry.value(1).toString();
                    scrollX = qry.value(3).toInt();
                    scrollY = qry.value(4).toInt();
                    icon = qry.value(5).toString();
                    if(icon == ":/img/clipboard.svg")
                    {
                        window->clip = true;
                        window->updateClipButton();
                        QString app;
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
                        if(qry.value(2).toInt() != 0) // is current page?
                        {
                            currentPage = page;
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

                    if(qry.value(2).toInt() != 0) // is current page?
                    {
                        currentPage = page;
                        if(url.isEmpty() == false || title.isEmpty() == false)
                        {
                            view->delayScroll(QPoint(scrollX, scrollY));
                            view->navigateTo(url);
                            view->setFocus();
                        }
                    }
                    else
                    {
                        if(url.isEmpty() == false || title.isEmpty() == false)
                        {
                            view->delayLoad(url, title, scrollX, scrollY);
                        }
                        window->tabWidget()->setTabToolTip(page, title);
                    }

                    view->setIcon(icon);
                    page++;
                }
                window->tabWidget()->setCurrentIndex(currentPage);
                window->tabWidget()->insertAfter = currentPage;
            }
            window->show();
        }
    }

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

    saveSettings();
    sigtermNotifier->setEnabled(true);
}

Browser::~Browser()
{
    saveSettings();
    closeAll();

    QSqlDatabase::removeDatabase(ds->connectionName);
}

BrowserWindow* Browser::createWindow()
{
    BrowserWindow* window = new BrowserWindow();
    window->browser = this;
    windows.append(window);
    connect(window, &QObject::destroyed, [this, window]()
    {
        windows.removeOne(window);
    });
    return window;
}

void Browser::closeAll()
{
    foreach(BrowserWindow* window, windows)
    {
        window->deleteLater();
    }
    windows.clear();
}

bool Browser::isWayland()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if(env.contains("XDG_SESSION_TYPE"))
    {
        QString sessionType = env.value("XDG_SESSION_TYPE").toLatin1();
        if(sessionType == "wayland")
        {
            return true;
        }
    }
    else if(env.contains("WAYLAND_DISPLAY"))
    {
        return true;
    }

    return false;
}

void Browser::saveSettings()
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    QSqlQuery qwindow(db);

    db.transaction();
    qry.exec("delete from TAB");
    qry.exec("delete from WINDOW");

    CompositeView* view;
    qwindow.prepare("insert into WINDOW (WINDOWID, X, Y, W, H) values (?, ?, ?, ?, ?)");
    qry.prepare("insert into TAB (WINDOWID, TABID, URL, TITLE, SCROLLX, SCROLLY, CURRENTPAGE, ICON) values (?, ?, ?, ?, ?, ?, ?, ?)");

    int windowId = 0;
    int tabId = 0;
    int tabCount;
    QRect r;
    foreach(BrowserWindow* window, windows)
    {
        if(isWayland())
        {
            r = window->frameGeometry();
        }
        else
        {
            r = window->geometry();
        }

        qwindow.bindValue(0, windowId);
        qwindow.bindValue(1, r.x());
        qwindow.bindValue(2, r.y());
        qwindow.bindValue(3, r.width());
        qwindow.bindValue(4, r.height());
        if(qwindow.exec() == false)
        {
            qDebug() << "Could not execute query:" << qwindow.executedQuery();
        }

        tabCount = window->tabWidget()->count();
        for(int i = 0; i < tabCount; i++)
        {
            view = window->tabWidget()->viewAt(i);

            QPoint scroll = view->scrollPosition();
            qry.bindValue(0, windowId);
            qry.bindValue(1, tabId++);

            if(view == window->installView)
            {
                qry.bindValue(2, "install: " + window->installList.join(' '));
            }
            else if(view == window->uninstallView)
            {
                qry.bindValue(2, "uninstall: " + window->uninstallList.join(' '));
            }
            else
            {
                qry.bindValue(2, view->url());
            }
            qry.bindValue(3, view->title());
            qry.bindValue(4, scroll.x());
            qry.bindValue(5, scroll.y());
            qry.bindValue(6, (i == window->tabWidget()->currentIndex()));
            qry.bindValue(7, view->iconFileName);
            if(qry.exec() == false)
            {
                qDebug() << "Could not execute query:" << qry.executedQuery();
            }
        }

        windowId++;
    }
    db.commit();
    db.close();
}

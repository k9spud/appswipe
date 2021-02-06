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

#include "browser.h"
#include "browserwindow.h"
#include "tabwidget.h"
#include "browserview.h"
#include "datastorage.h"

#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QIcon>
#include <QDebug>

Browser::Browser(QObject *parent) : QObject(parent)
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    QSqlQuery qwindow(db);
    qry.prepare("select URL, TITLE, CURRENTPAGE, SCROLLX, SCROLLY, ICON from TAB where WINDOWID=? order by TABID");

    int windowId;
    BrowserWindow* window;
    BrowserView* view;
    QString url;
    QString title;
    QString icon;
    int scrollX, scrollY;
    int currentPage;
    int page;
    if(qwindow.exec("select WINDOWID, X, Y, W, H from WINDOW order by WINDOWID"))
    {
        while(qwindow.next())
        {
            windowId = qwindow.value(0).toInt();
            window = createWindow();
            window->setGeometry(qwindow.value(1).toInt(), qwindow.value(2).toInt(), qwindow.value(3).toInt(), qwindow.value(4).toInt());

            qry.bindValue(0, windowId);
            if(qry.exec())
            {
                page = 0;
                currentPage = 0;
                while(qry.next())
                {
                    if(page == 0)
                    {
                        view = window->currentView();
                    }
                    else
                    {
                        view = window->tabWidget()->createTab();
                    }

                    url = qry.value(0).toString();
                    title = qry.value(1).toString();
                    scrollX = qry.value(3).toInt();
                    scrollY = qry.value(4).toInt();
                    icon = qry.value(5).toString();
                    if(qry.value(2).toInt() != 0) // is current page?
                    {
                        currentPage = page;
                        view->delayScroll(scrollX, scrollY);
                        view->navigateTo(url);
                        view->setFocus();
                    }
                    else
                    {
                        view->delayLoad(url, title, scrollX, scrollY);
                        window->tabWidget()->setTabToolTip(page, title);
                    }

                    view->setIcon(icon);
                    page++;
                }
                window->tabWidget()->setCurrentIndex(currentPage);
            }
            window->show();
        }
    }
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
        while(window->tabWidget()->count() > 1)
        {
            window->tabWidget()->closeTab(0);
        }
        window->close();
        qApp->processEvents();
    }
}

void Browser::saveSettings()
{
    QSqlDatabase db = QSqlDatabase::database(ds->connectionName);
    QSqlQuery qry(db);
    QSqlQuery qwindow(db);

    db.transaction();
    qry.exec("delete from TAB");
    qry.exec("delete from WINDOW");

    BrowserView* view;
    qwindow.prepare("insert into WINDOW (WINDOWID, X, Y, W, H) values (?, ?, ?, ?, ?)");
    qry.prepare("insert into TAB (WINDOWID, TABID, URL, TITLE, SCROLLX, SCROLLY, CURRENTPAGE, ICON) values (?, ?, ?, ?, ?, ?, ?, ?)");

    int windowId = 0;
    int tabId = 0;
    int tabCount;
    QRect r;
    foreach(BrowserWindow* window, windows)
    {
        r = window->geometry();
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
            view = window->tabWidget()->tabView(i);

            QPoint scroll = view->scrollPosition();
            qry.bindValue(0, windowId);
            qry.bindValue(1, tabId++);
            qry.bindValue(2, view->url());
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

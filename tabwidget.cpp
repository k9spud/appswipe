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

#include "tabwidget.h"
#include "k9tabbar.h"
#include "browserview.h"
#include "browserwindow.h"

#include <QIcon>
#include <QPixmap>
#include <QTransform>
#include <QDebug>

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent)
{
    window = nullptr;

    tabbar = new K9TabBar(this);
    setTabBar(tabbar);
    tabbar->setTabsClosable(false);
    tabbar->setDocumentMode(true);
    tabbar->setElideMode(Qt::ElideRight);
    tabbar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    tabbar->setMovable(true);
    tabbar->setContextMenuPolicy(Qt::CustomContextMenu);
    tabbar->setAutoHide(true);
    connect(tabbar, &QTabBar::tabCloseRequested, this, &TabWidget::closeTab);
    connect(tabbar, &QTabBar::tabBarDoubleClicked, [this](int index)
    {
        if(index == -1)
        {
            createTab();
            window->focusLineEdit();
        }
    });
    connect(this, &QTabWidget::currentChanged, this, &TabWidget::currentTabChanged);
}

BrowserView* TabWidget::currentView()
{
    int index = currentIndex();
    if(index >= 0)
    {
        return tabView(index);
    }

    return createTab();
}

BrowserView* TabWidget::tabView(int index)
{
    if(index >= 0 && index < count())
    {
        BrowserView* view = qobject_cast<BrowserView*>(widget(index));
        if(view != nullptr)
        {
            return view;
        }
    }

    return createTab();
}

void TabWidget::setTabIcon(int index, const QIcon& icon)
{
    QIcon::Mode mode = QIcon::Normal;
    BrowserView* view = tabView(index);
    if(view != nullptr)
    {
        if(view->delayLoading)
        {
            mode = QIcon::Disabled;
        }
    }

    QSize sz;
    sz.setHeight(32);
    sz.setWidth(32);

    QPixmap pix = icon.pixmap(sz, mode);
    if(tabPosition() == QTabWidget::East)
    {
        QTransform trans;
        trans.rotate(-90);
        pix = pix.transformed(trans);
    }
    else if(tabPosition() == QTabWidget::West)
    {
        QTransform trans;
        trans.rotate(+90);
        pix = pix.transformed(trans);
    }

    QIcon roticon = QIcon(pix);
    QTabWidget::setTabIcon(index, roticon);
}

BrowserView* TabWidget::createTab()
{
    BrowserView* view = createBackgroundTab();
    setCurrentWidget(view);
    return view;
}

void TabWidget::openInNewTab(const QString& url)
{
    BrowserView* view = createBackgroundTab();
    if(url.isEmpty() == false)
    {
        view->navigateTo(url);
    }
}

BrowserView* TabWidget::createBackgroundTab()
{
    BrowserView* view = new BrowserView();
    view->window = window;

    connect(view, &BrowserView::openInNewTab, this, &TabWidget::openInNewTab);
    connect(view, &BrowserView::titleChanged, [this, view](const QString& title)
    {
        int index = indexOf(view);
        if(index >= 0)
        {
            setTabToolTip(index, title);
        }

        if(index == currentIndex())
        {
            emit titleChanged(title);
        }
    });

    connect(view, &BrowserView::urlChanged, [this, view](const QUrl& url)
    {
        int index = indexOf(view);
        if(index >= 0)
        {
            tabbar->setTabData(index, url);
        }

        if(index == currentIndex())
        {
            emit urlChanged(url);
        }
    });

    int index = addTab(view, "");
    setTabIcon(index, view->icon());

    connect(view, &BrowserView::iconChanged, [this, view](QIcon icon)
    {
        if(currentView() == view)
        {
            setTabIcon(currentIndex(), icon);
        }
        else
        {
            for(int i = 0; i < count(); i++)
            {
                if(widget(i) == view)
                {
                    setTabIcon(i, icon);
                    break;
                }
            }
        }
    });
    return view;
}

void TabWidget::closeTab(int index)
{
    BrowserView* view = tabView(index);
    if(view != nullptr)
    {
        bool hasFocus = view->hasFocus();
        removeTab(index);
        if(hasFocus && count() > 0)
        {
            view->setFocus();
        }

        if(count() == 0)
        {
            createTab();
        }
        view->deleteLater();
    }
}

void TabWidget::closeAll()
{
    disconnect(this, &QTabWidget::currentChanged, this, &TabWidget::currentTabChanged);
    for(int i = 0; i < count(); i++)
    {
        widget(i)->deleteLater();
    }
    clear();
}

void TabWidget::currentTabChanged(int index)
{
    tabbar->showTabLabel(-1);

    if(index == -1)
    {
        emit titleChanged(QString());
        emit urlChanged(QUrl());
        return;
    }

    BrowserView* view = tabView(index);
    if(view == nullptr)
    {
        return;
    }

    if(view->delayLoading)
    {
        view->load();
    }

    if(!view->url().isEmpty())
    {
        view->setFocus();
    }
    else
    {
        window->focusLineEdit();
    }

    emit titleChanged(view->documentTitle());
    emit urlChanged(view->url());
}

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

#include "tabwidget.h"
#include "k9tabbar.h"
#include "browserview.h"
#include "browserwindow.h"
#include "history.h"
#include "compositeview.h"

#include <QIcon>
#include <QPixmap>
#include <QTransform>
#include <QDebug>

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent)
{
    window = nullptr;
    oldIndex = -1;
    oldView = nullptr;
    insertAfter = -1;

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
    connect(tabbar, &QTabBar::tabBarDoubleClicked, this, [this](int index)
    {
        if(index == -1)
        {
            createTab();
            window->focusLineEdit();
        }
    });
    connect(this, &QTabWidget::currentChanged, this, &TabWidget::currentTabChanged);
}

CompositeView* TabWidget::currentView()
{
    int index = currentIndex();
    if(index >= 0 && index < count())
    {
        return qobject_cast<CompositeView*>(widget(index));
    }

    return createTab();
}

CompositeView* TabWidget::viewAt(int index)
{
    if(index >= 0 && index < count())
    {
        CompositeView* view = qobject_cast<CompositeView*>(widget(index));
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
    CompositeView* view = qobject_cast<CompositeView*>(widget(index));
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

void TabWidget::forward()
{
    currentView()->forward();
}

void TabWidget::back()
{
    currentView()->back();
}

QMenu* TabWidget::forwardMenu()
{
    CompositeView* view = currentView();
    if(view == nullptr)
    {
        return nullptr;
    }
    return view->history->forwardMenu();
}

QMenu* TabWidget::backMenu()
{
    CompositeView* view = currentView();
    if(view == nullptr)
    {
        return nullptr;
    }
    return view->history->backMenu();
}

CompositeView* TabWidget::createTab()
{
    CompositeView* view = createBackgroundTab();
    setCurrentWidget(view);
    return view;
}

CompositeView* TabWidget::createEmptyTab()
{
    CompositeView* view = createBackgroundTab();
    setCurrentWidget(view);
    return view;
}

void TabWidget::openInNewTab(const QString& url)
{
    CompositeView* view = createBackgroundTab();

    if(url.isEmpty() == false)
    {
        view->navigateTo(url);
    }
}

CompositeView* TabWidget::createBackgroundTab(int insertIndex)
{
    K9TabBar* tb = qobject_cast<K9TabBar*>(tabBar());
    QRect before = tb->tabRect(0);

    CompositeView* view = createView();

    int index;
    if(insertIndex == -1)
    {
        insertAfter++;
        index = insertTab(insertAfter, view, "");
    }
    else if(insertIndex >= 0)
    {
        index = insertTab(insertIndex + 1, view, "");
    }
    else
    {
        index = addTab(view, "");
    }
    setTabIcon(index, view->icon());

    QRect after = tb->tabRect(0);
    if(before.y() < after.y())
    {
        int ticks = (after.y() - before.y()) / after.height() + 1;
        tb->scrollDown(ticks);
        after = tb->tabRect(0);
        if(before.y() > after.y())
        {
            tb->scrollUp();
        }
    }
    else if(before.y() > after.y())
    {
        int ticks = (before.y() - after.y()) / after.height() + 1;
        tb->scrollUp(ticks);
        after = tb->tabRect(0);
        if(before.y() < after.y())
        {
            tb->scrollDown();
        }
    }

    return view;
}

void TabWidget::connectView(CompositeView* view)
{
    view->window = window;
    connect(view, &CompositeView::openInNewTab, this, &TabWidget::openInNewTab);
    connect(view, &CompositeView::titleChanged, this, [this, view](const QString& title)
    {
        int index = indexOf(view);
        if(index == currentIndex())
        {
            emit titleChanged(title);
        }
    });

    connect(view, &CompositeView::urlChanged, this, [this, view](const QUrl& url)
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

    connect(view, &CompositeView::iconChanged, this, [this, view](QIcon icon)
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
}

CompositeView* TabWidget::createView()
{
    CompositeView* view = new CompositeView();
    connectView(view);
    return view;
}

void TabWidget::closeTab(int index)
{
    QWidget* w = widget(index);
    if(w == nullptr)
    {
        return; // no tab widget, nothing to do.
    }

    CompositeView* view = qobject_cast<CompositeView*>(w);
    if(view == window->installView)
    {
        window->installList.clear();
        window->installView = nullptr;
    }
    else if(view == window->uninstallView)
    {
        window->uninstallList.clear();
        window->uninstallView = nullptr;
    }

    bool hasFocus = view->hasFocus();
    K9TabBar* tb = qobject_cast<K9TabBar*>(tabBar());
    QRect before = tb->tabRect(0);

    if(index == currentIndex())
    {
        oldIndex = -1;
    }

    removeTab(index);
    if(count() == 0)
    {
        CompositeView* newView = createTab();
        if(hasFocus && count() > 0)
        {
            newView->setFocus();
        }
    }
    view->deleteLater();

    QRect after = tb->tabRect(0);
    if(before.y() < after.y())
    {
        int ticks = (after.y() - before.y()) / after.height() + 1;
        tb->scrollDown(ticks);
        after = tb->tabRect(0);
        if(before.y() > after.y())
        {
            tb->scrollUp();
        }
    }
    else if(before.y() > after.y())
    {
        int ticks = (before.y() - after.y()) / after.height() + 1;
        tb->scrollUp(ticks);
        after = tb->tabRect(0);
        if(before.y() < after.y())
        {
            tb->scrollDown();
        }
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
    CompositeView* view;

    tabbar->showTabLabel(-1);

    if(oldIndex >= 0 && oldIndex < count())
    {
        view = qobject_cast<CompositeView*>(widget(oldIndex));
        if(view != nullptr && view == oldView)
        {
            History::State state;
            state.target= window->lineEditText();
            state.title = view->title();
            state.pos = view->scrollPosition();
            view->history->updateState(state);
            disconnect(view->history, &History::enableChanged, window, &BrowserWindow::enableChanged);
            disconnect(view->history, &History::stateChanged, view, &CompositeView::stateChanged);
        }
    }
    oldIndex = index;
    oldView = qobject_cast<CompositeView*>(widget(index));

    if(index == -1)
    {
        emit titleChanged(QString());
        emit urlChanged(QUrl());
        emit enableChanged(History::Back, false);
        emit enableChanged(History::Forward, false);
        return;
    }

    insertAfter = index;

    view = qobject_cast<CompositeView*>(widget(index));
    if(view == nullptr)
    {
        return;
    }

    if(view->delayLoading)
    {
        view->load();
    }

    if(view->url().isEmpty())
    {
        window->focusLineEdit();
    }
    else
    {
        view->setFocus();
    }

    connect(view->history, &History::enableChanged, window, &BrowserWindow::enableChanged);
    connect(view->history, &History::stateChanged, view, &CompositeView::stateChanged);

    emit titleChanged(view->title());
    emit urlChanged(view->url());
    view->history->checkEnables();
}

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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include "browserview.h"
#include "history.h"

#include <QWidget>
#include <QTabWidget>
#include <QVector>

class K9TabBar;
class BrowserView;
class CompositeView;
class BrowserWindow;
class TabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = nullptr);

    BrowserWindow* window;
    CompositeView* currentView();
    CompositeView* viewAt(int index);
    void connectView(CompositeView* view);

    virtual void setTabIcon(int index, const QIcon& icon);

    int insertAfter;

    void forward();
    void back();

    QMenu* forwardMenu();
    QMenu* backMenu();
    
public slots:
    CompositeView* createTab();
    CompositeView* createEmptyTab();
    CompositeView* createBackgroundTab(int insertIndex = -1);
    CompositeView* createView();
    void openInNewTab(const QString& url);
    void closeTab(int index);
    void closeAll();

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);

    void enableChanged(History::WebAction action, bool enabled);

private slots:
    void currentTabChanged(int index);

private:
    K9TabBar* tabbar;
    int oldIndex;
    CompositeView* oldView;
};

#endif // TABWIDGET_H

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

#ifndef COMPOSITEVIEW_H
#define COMPOSITEVIEW_H

#include "history.h"

#include <QWidget>
#include <QIcon>

class BrowserWindow;
class BrowserView;
class ImageView;
class QLabel;
class CompositeView : public QWidget
{
    Q_OBJECT

public:
    explicit CompositeView(QWidget* parent = nullptr);
    ~CompositeView();

    QString url();
    QString title() const;
    QIcon icon();
    void setIcon(QString fileName);
    QIcon siteIcon;
    QString iconFileName;

    void delayLoad(QString url, QString title, int scrollX, int scrollY);
    void delayScroll(QPoint pos);
    void load();

    bool delayLoading;
    bool delayScrolling;
    History::State delayState;

    QPoint scrollPosition();
    void setScrollPosition(const QPoint& pos);
    void saveScrollPosition();

    History::State* currentState();
    History* history;
    BrowserWindow* window;

    void setText(const QString& text);
    void clear();

    BrowserView* browser();
    ImageView* image();

    void discardView(QWidget* view);

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void iconChanged(const QIcon &icon);
    void loadStarted();
    void loadProgress(int percent);
    void openInNewTab(const QString& url);

public slots:
    void setTitle(QString newTitle);
    void setStatus(QString text);
    QString status();

    void forward();
    void back();
    void stateChanged(const History::State& s);
    void navigateTo(QString text, bool changeHistory = true, bool feelingLucky = false);
    void jumpTo(const History::State& s);
    void setUrl(const QUrl& url);
    void error(QString text);
    void reload(bool hardReload = true);
    void reloadingDatabase();
    void applyDelayedScroll();

protected:
    BrowserView* browserView;
    ImageView* imageView;

    QLabel* statusLabel;
    void resizeStatusBar();
    virtual void resizeEvent(QResizeEvent* event) override;

    friend BrowserView;
};

#endif // COMPOSITEVIEW_H

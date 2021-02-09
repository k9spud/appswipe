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

#ifndef BROWSERVIEW_H
#define BROWSERVIEW_H

#include <QTextEdit>
#include <QStringList>

#include <QHash>
#include <QVariant>
#include <QRegularExpression>
#include <QIcon>
#include <QPointF>

class QLabel;
class QMenu;
class QContextMenuEvent;
class QSqlQuery;
class BrowserWindow;
class BrowserView : public QTextEdit
{
    Q_OBJECT
public:
    explicit BrowserView(QWidget *parent = nullptr);

    QMenu* createStandardContextMenu(const QPoint &position);

    void appendHistory(QString text);
    QStringList history;
    int currentHistory;
    QString url();
    QString title() const;
    QPoint scrollPosition();
    void setScrollPosition(int x, int y);
    QIcon icon();
    void setIcon(QString fileName);
    QIcon siteIcon;
    QString iconFileName;

    void load();
    void delayLoad(QString url, QString title, int scrollX, int scrollY);
    void delayScroll(int scrollX, int scrollY);
    bool delayLoading;
    bool delayScrolling;
    int delayScrollX, delayScrollY;

    BrowserWindow* window;

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void iconChanged(const QIcon &icon);
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished(bool ok);
    void openInNewTab(const QString& url);

public slots:
    void setStatus(QString text);
    void navigateTo(QString text, bool changeHistory = true, bool feelingLucky = false);
    void setUrl(const QUrl& url);
    void error(QString text);
    void back();
    void forward();
    void reload(bool hardReload = true);
    void stop();

    void about();
    void reloadingDatabase();
    void viewFile(QString fileName);
    void viewApp(const QUrl& url);
    void reloadApp(const QUrl& url);
    void searchApps(QString search, bool feelingLucky = false);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    virtual void contextMenuEvent(QContextMenuEvent *event) override;

    QHash<QString, QString> iconMap;

    QString delayUrl;
    QString delayTitle;

    QLabel* status;
    QString context;

    void printApp(QString& result, QHash<QString, QString>& installedVersions, QHash<QString, QString>& obsoletedVersions, QStringList& apps, QSqlQuery& query);
};

#endif // BROWSERVIEW_H

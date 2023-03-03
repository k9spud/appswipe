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

#ifndef BROWSER_H
#define BROWSER_H

#include <QObject>
#include <QVector>
#include <QSocketNotifier>

class BrowserWindow;
class QSqlQuery;
class Browser : public QObject
{
    Q_OBJECT
public:
    explicit Browser(QObject *parent = nullptr);
    ~Browser();

    struct WindowHash
    {
        int windowId;
        QString title;
    };
    QVector<WindowHash> inactiveWindows();

    enum WindowStatus
    {
        Inactive = 0,
        Active   = 1,
        Maximized =1<<2
    };

    void restoreWindows();
    void loadWindows(QSqlQuery& windowQuery, QSqlQuery& tabQuery);
    void loadWindow(int windowId);

    QVector<BrowserWindow*> windows;

    BrowserWindow* createWindow(int windowId = -1);
    void closeAll();
    int createWindowId();
    void saveWindow(BrowserWindow* window, bool active = true);
    void discardWindow(int windowId);

    // Unix signal handlers
    static void unixSIGHUP(int unused);
    static void unixSIGTERM(int unused);

public slots:
    // Qt signal handlers
    void handleSIGHUP(void);
    void handleSIGTERM(void);

    void saveAllWindows();

signals:

private:
    QSocketNotifier* sighupNotifier;
    QSocketNotifier* sigtermNotifier;

    bool discardWindow(int windowId, QSqlQuery& query);
    void saveWindow(BrowserWindow* window, bool active, QSqlQuery& qwindow, QSqlQuery& query);

};

#endif // BROWSER_H

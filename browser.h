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

#ifndef BROWSER_H
#define BROWSER_H

#include <QObject>
#include <QVector>

class BrowserWindow;
class Browser : public QObject
{
    Q_OBJECT
public:
    explicit Browser(QObject *parent = nullptr);
    ~Browser();

    QVector<BrowserWindow*> windows;

    BrowserWindow *createWindow();
    void closeAll();
    void saveSettings();

signals:

};

#endif // BROWSER_H

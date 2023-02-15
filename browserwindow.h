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

#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include "history.h"

#include <QMainWindow>
#include <QSize>

QT_BEGIN_NAMESPACE
namespace Ui { class BrowserWindow; }
QT_END_NAMESPACE

class QMenu;
class QResizeEvent;
class RescanThread;
class BrowserView;
class CompositeView;
class TabWidget;
class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget* parent = nullptr);
    ~BrowserWindow();

    int windowId;
    bool clip;
    bool ask;

    QSize unmaximizedSize;

    CompositeView* installView;
    CompositeView* uninstallView;
    QStringList installList;
    QStringList uninstallList;

    TabWidget* tabWidget();
    CompositeView* currentView();

    QString lineEditText();

public slots:
    void updateAskButton();
    void updateClipButton();
    void focusLineEdit();
    void exec(QString cmd);
    void exec(QString cmd, QString title);
    void install(QString atom, bool isWorld);
    void uninstall(QString atom);
    BrowserWindow* openWindow();
    void reloadDatabase();
    void reloadDatabaseComplete();
    void viewUpdates();
    void switchToTab(int index);
    void enableChanged(History::WebAction action, bool enabled);

protected slots:

protected:
    void closeEvent(QCloseEvent *event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;

private slots:
    void on_menuButton_pressed();
    void on_lineEdit_returnPressed();
    void workFinished();

    void back();
    void forward();
    void stop();

    void on_newTabButton_clicked();
    void on_newTabButton_longPressed();
    void on_reloadButton_clicked();
    void on_askButton_clicked();
    void on_clipButton_clicked();

    void on_backButton_longPressed();
    void on_forwardButton_longPressed();

private:
    Ui::BrowserWindow *ui;

    QMenu* menu;
    QMenu* tabsMenu;
    QMenu* inactiveWindows;

    void setWorking(bool workin);
    bool working;
    QString lastSearch;
};
#endif // BROWSERWINDOW_H

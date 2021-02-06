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

#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class BrowserWindow; }
QT_END_NAMESPACE

class QMenu;
class RescanThread;
class Browser;
class BrowserView;
class TabWidget;
class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget* parent = nullptr);
    ~BrowserWindow();

    Browser* browser;

    bool clip;
    bool ask;

    QStringList installList;
    QStringList uninstallList;

    TabWidget* tabWidget();
    BrowserView* currentView();

public slots:
    void updateAskButton();
    void updateClipButton();
    void exec(QString cmd);
    void install(QString atom);
    void uninstall(QString atom);
    void openWindow();
    void reloadDatabase();
    void reloadDatabaseComplete();

protected slots:

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_menuButton_pressed();
    void on_lineEdit_returnPressed();
    void workFinished();

    void stop();

    void back();
    void forward();

    void on_newTabButton_clicked();
    void on_reloadButton_clicked();
    void on_askButton_clicked();
    void on_clipButton_clicked();

private:
    Ui::BrowserWindow *ui;

    QMenu* menu;

    void setWorking(bool workin);
    bool working;
    QString lastSearch;

    BrowserView* installView;
    BrowserView* uninstallView;
};
#endif // BROWSERWINDOW_H

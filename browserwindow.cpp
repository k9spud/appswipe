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

#include "browserwindow.h"
#include "ui_browserwindow.h"
#include "globals.h"
#include "rescanthread.h"
#include "main.h"
#include "browser.h"
#include "compositeview.h"
#include "tabwidget.h"
#include "k9shell.h"

#include <QStringList>
#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QClipboard>
#include <QInputDialog>
#include <QCloseEvent>
#include <QMessageBox>

BrowserWindow::BrowserWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::BrowserWindow)
{
    setFocusPolicy(Qt::ClickFocus);

    ui->setupUi(this);

    installView = nullptr;
    uninstallView = nullptr;
    clip = false;
    ask = true;
    ui->tabWidget->window = this;

    QAction *action = new QAction(this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(action, &QAction::triggered, this, [this]()
    {
        if(ui->lineEdit->hasFocus())
        {
            ui->lineEdit->selectAll();
        }
        else
        {
            ui->lineEdit->setFocus(Qt::ShortcutFocusReason);
        }
    });
    addAction(action);

    action = new QAction(this);
    action->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    connect(action, &QAction::triggered, this, [this]()
    {
        bool feelingLucky = true;
        QString s = ui->lineEdit->text();
        ui->tabWidget->currentView()->navigateTo(s, true, feelingLucky);
        ui->tabWidget->currentView()->setFocus();
        ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
    });
    addAction(action);

    action = new QAction("New &Tab", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(action, &QAction::triggered, this, &BrowserWindow::on_newTabButton_clicked);
    addAction(action);

    action = new QAction("Tab 1", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(1);
    });
    addAction(action);

    action = new QAction("Tab 2", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(2);
    });
    addAction(action);

    action = new QAction("Tab 3", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(3);
    });
    addAction(action);

    action = new QAction("Tab 4", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(4);
    });
    addAction(action);

    action = new QAction("Tab 5", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(5);
    });
    addAction(action);

    action = new QAction("Tab 6", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(6);
    });
    addAction(action);

    action = new QAction("Tab 7", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_7));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(7);
    });
    addAction(action);

    action = new QAction("Tab 8", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_8));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(8);
    });
    addAction(action);

    action = new QAction("Tab 9", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_9));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(9);
    });
    addAction(action);

    action = new QAction("Last Tab", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(action, &QAction::triggered, this, [this]()
    {
        switchToTab(0);
    });
    addAction(action);

    action = new QAction("&New Window", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(action, &QAction::triggered, this, &BrowserWindow::openWindow);
    addAction(action);

    action = new QAction(tr("&Close Tab"), this);
    action->setShortcuts(QKeySequence::Close);
    connect(action, &QAction::triggered, [this]()
    {
        if(ui->tabWidget->count() <= 1)
        {
            close();
        }
        else
        {
            ui->tabWidget->closeTab(ui->tabWidget->currentIndex());
        }
    });
    addAction(action);

    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_R));
    shortcuts.append(QKeySequence(Qt::Key_F5));
    action = new QAction(tr("&Reload"), this);
    action->setShortcuts(shortcuts);
    connect(action, &QAction::triggered, this, &BrowserWindow::on_reloadButton_clicked);
    addAction(action);

    shortcuts.clear();
    shortcuts.append(QKeySequence::keyBindings(QKeySequence::Back));
    action = new QAction(tr("&Back"), this);
    action->setShortcuts(shortcuts);
    connect(action, &QAction::triggered, this, &BrowserWindow::back);
    connect(ui->backButton, &K9PushButton::clicked, this, &BrowserWindow::back);
    addAction(action);

    shortcuts.clear();
    shortcuts.append(QKeySequence::keyBindings(QKeySequence::Forward));
    action = new QAction(tr("&Forward"), this);
    action->setShortcuts(shortcuts);
    connect(action, &QAction::triggered, this, &BrowserWindow::forward);
    connect(ui->forwardButton, &K9PushButton::clicked, this, &BrowserWindow::forward);
    addAction(action);

    connect(ui->tabWidget, &TabWidget::titleChanged, this, [this](const QString& title)
    {
        if(title.isEmpty())
        {
            setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));
        }
        else
        {
            setWindowTitle(QString("%1 - %2 v%3").arg(title).arg(APP_NAME).arg(APP_VERSION));
        }
    });

    connect(ui->tabWidget, &TabWidget::urlChanged, this, [this](const QUrl& url)
    {
        QString s = url.toDisplayString();
        ui->lineEdit->setText(s);
    });

    action = new QAction(tr("&Stop"), this);
    action->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(action, &QAction::triggered, this, &BrowserWindow::stop);
    addAction(action);

    menu = new QMenu("App Menu", this);
    menu->setStyleSheet("background-color: #393939;");

    action = new QAction(this);
    action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F));
    connect(action, &QAction::triggered, this, &BrowserWindow::on_menuButton_pressed);
    addAction(action);

    action = new QAction("News", this);
    connect(action, &QAction::triggered, this, []()
    {
        // might want to browse /var/db/repos/gentoo/metadata/news/ instead
        QString cmd = "eselect news read";
        shell->externalTerm(cmd, "News");
    });
    menu->addAction(action);

    action = new QAction("Security Advisories", this);
    connect(action, &QAction::triggered, this, []()
    {
        QString cmd = "glsa-check -l";
        shell->externalTerm(cmd, "Security Advisories");
    });
    menu->addAction(action);

    action = new QAction("Sync Repos", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --sync --nospinner";
        if(ask)
        {
            cmd.append(" --ask");
        }
        cmd.append(QString("\nexport RET_CODE=$?\n%1 -synced -pid %2").arg(qApp->applicationFilePath()).arg(qApp->applicationPid()));

        exec(cmd, "Sync Repos");
    });
    menu->addAction(action);

    action = new QAction("Reload Database", this);
    connect(action, &QAction::triggered, this, &BrowserWindow::reloadDatabase);
    menu->addAction(action);

    action = new QAction("View Updates", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    connect(action, &QAction::triggered, this, &BrowserWindow::viewUpdates);
    menu->addAction(action);
    addAction(action);

    action = new QAction("Fetch Used World", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --update @world --fetchonly --newuse --deep --with-bdeps=y --nospinner ";
        exec(cmd, "Fetch Used World");
    });
    menu->addAction(action);

    action = new QAction("Fetch All World", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --update @world --fetch-all-uri --newuse --deep --with-bdeps=y --nospinner";
        exec(cmd, "Fetch All World");
    });
    menu->addAction(action);

    action = new QAction("Update System", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --update @system --newuse --deep --with-bdeps=y --verbose --verbose-conflicts --nospinner";
        if(ask)
        {
            cmd.append(" --ask");
        }
        exec(cmd, "Update System");
    });
    menu->addAction(action);

    action = new QAction("Update World", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --update @world --newuse --deep --with-bdeps=y --verbose --verbose-conflicts --nospinner";
        if(ask)
        {
            cmd.append(" --ask");
        }
        exec(cmd, "Update World");
    });
    menu->addAction(action);

    action = new QAction("Dispatch Config Changes", this);
    connect(action, &QAction::triggered, this, []()
    {
        QString cmd = "sudo dispatch-conf";
        shell->externalTerm(cmd, "Dispatch Config Changes");
    });
    menu->addAction(action);

    action = new QAction("Clean Unused Dependencies", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "sudo emerge --depclean --deep --with-bdeps=y --verbose --verbose-conflicts --nospinner";
        if(ask)
        {
            cmd.append(" --ask");
        }
        exec(cmd, "Clean Unused Dependencies");
    });
    menu->addAction(action);


    action = new QAction("Exit", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(action, &QAction::triggered, this, []()
    {
        qApp->quit();
    });
    menu->addAction(action);
    addAction(action);

    setWorking(false);
    setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));
    setWindowTitle(APP_NAME);
    updateAskButton();
    updateClipButton();

    ui->backButton->setVisible(false);
    ui->forwardButton->setVisible(false);
    ui->lineEdit->setFocus();
}

BrowserWindow::~BrowserWindow()
{
    ui->tabWidget->closeAll();
    delete ui;
}

TabWidget* BrowserWindow::tabWidget()
{
    return ui->tabWidget;
}

CompositeView* BrowserWindow::currentView()
{
    return ui->tabWidget->currentView();
}

QString BrowserWindow::lineEditText()
{
    return ui->lineEdit->text();
}

void BrowserWindow::back()
{
    ui->tabWidget->back();
}

void BrowserWindow::forward()
{
    ui->tabWidget->forward();
}

void BrowserWindow::on_lineEdit_returnPressed()
{
    bool feelingLucky = false;
    if(qApp->keyboardModifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier))
    {
       feelingLucky = true;
    }
    QString s = ui->lineEdit->text();
    ui->tabWidget->currentView()->navigateTo(s, true, feelingLucky);
    ui->tabWidget->currentView()->setFocus();
    ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
}

void BrowserWindow::workFinished()
{
    ui->searchProgress->setVisible(false);
    ui->lineEdit->setVisible(true);
    currentView()->setStatus("");
    setWorking(false);
}

void BrowserWindow::on_menuButton_pressed()
{
    QPoint point = mapToGlobal(ui->menuButton->pos());
    point.setX(point.x() - menu->sizeHint().width() + ui->menuButton->width());
    point.setY(point.y() + ui->menuButton->height());
    menu->exec(point);
}

void BrowserWindow::on_reloadButton_clicked()
{
    if(working)
    {
        stop();
    }
    else
    {
        setWorking(true);
        CompositeView* view = ui->tabWidget->currentView();
        connect(view, SIGNAL(loadProgress(int)), ui->searchProgress, SLOT(setValue(int)));

        bool hardReload = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier) ||
                          QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier);
        view->reload(hardReload);
        ui->lineEdit->setText(view->url());
        disconnect(view, SIGNAL(loadProgress(int)), ui->searchProgress, SLOT(setValue(int)));
        setWorking(false);
        ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), view->icon());
    }
}

void BrowserWindow::stop()
{
    if(rescan != nullptr)
    {
        rescan->abort = true;
    }
    setWorking(false);
}

void BrowserWindow::enableChanged(History::WebAction action, bool enabled)
{
    switch (action)
    {
        case History::Back:
            ui->backButton->setVisible(enabled);
            break;

        case History::Forward:
            ui->forwardButton->setVisible(enabled);
            break;

        default:
            break;
    }
}

void BrowserWindow::setWorking(bool workin)
{
    static QIcon stopIcon(":/img/ic_cancel_48px.svg");
    static QIcon reloadIcon(":/img/ic_refresh_48px.svg");

    if(workin)
    {
        ui->reloadButton->setIcon(stopIcon);
        working = true;

        ui->searchProgress->setVisible(true);
        ui->lineEdit->setVisible(false);
    }
    else
    {
        ui->reloadButton->setIcon(reloadIcon);
        working = false;

        ui->searchProgress->setVisible(false);
        ui->lineEdit->setVisible(true);
    }
}

void BrowserWindow::on_newTabButton_clicked()
{
    ui->tabWidget->createEmptyTab();
    ui->lineEdit->clear();
    ui->lineEdit->setFocus();
}

void BrowserWindow::on_askButton_clicked()
{
    ask = !ask;
    updateAskButton();
}

void BrowserWindow::on_clipButton_clicked()
{
    clip = !clip;
    updateClipButton();
}

void BrowserWindow::on_backButton_longPressed()
{
    QMenu* menu = tabWidget()->backMenu();
    if(menu != nullptr)
    {
        QPoint point = mapToGlobal(ui->backButton->pos());
        QPoint p(point.x(), point.y() + ui->backButton->height());
        menu->exec(p);
    }
}

void BrowserWindow::on_forwardButton_longPressed()
{
    QMenu* menu = tabWidget()->forwardMenu();
    if(menu != nullptr)
    {
        QPoint point = mapToGlobal(ui->forwardButton->pos());
        QPoint p(point.x(), point.y() + ui->forwardButton->height());
        menu->exec(p);
    }
}

void BrowserWindow::updateAskButton()
{
    QIcon icon(":/img/jean_victor_balin_unknown_green.svg");
    QIcon::Mode mode = QIcon::Normal;
    if(ask == false)
    {
        mode = QIcon::Disabled;
    }
    QSize sz(32, 32);
    QPixmap pix = icon.pixmap(sz, mode);
    ui->askButton->setIcon(QIcon(pix));
}

void BrowserWindow::updateClipButton()
{
    QIcon icon(":/img/clipboard.svg");
    QIcon::Mode mode = QIcon::Normal;
    if(clip == false)
    {
        mode = QIcon::Disabled;
    }
    QSize sz(32, 32);
    QPixmap pix = icon.pixmap(sz, mode);
    ui->clipButton->setIcon(QIcon(pix));
}

void BrowserWindow::focusLineEdit()
{
    ui->lineEdit->setFocus();
}

void BrowserWindow::exec(QString cmd)
{
    if(clip)
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(cmd);
    }
    else
    {
        shell->externalTerm(cmd);
    }
}

void BrowserWindow::exec(QString cmd, QString title)
{
    if(clip && (title.endsWith("install") || title.endsWith("uninstall")))
    {
        QClipboard* clip = qApp->clipboard();
        clip->setText(cmd);
    }
    else
    {
        shell->externalTerm(cmd, title);
    }
}

void BrowserWindow::install(QString atom, bool isWorld)
{
    if(clip == false)
    {
        return;
    }

    if(installView == nullptr)
    {
        installView = ui->tabWidget->createTab();

        History::State state;
        state.pos = QPoint(0, 0);
        state.target = "";
        state.title = "Apps to Install...";
        installView->history->appendHistory(state);
    }
    else
    {
        ui->tabWidget->setCurrentWidget(installView);
    }

    if(installList.contains(atom) == false)
    {
        installList.append(atom);
    }

    ui->lineEdit->clear();
    installView->browser()->clear();
    installView->setIcon(":/img/clipboard.svg");
    QString cmd = "sudo emerge";
    foreach(atom, installList)
    {
        cmd.append(QString(" =%1").arg(atom));
    }
    cmd.append(" --newuse --verbose --verbose-conflicts --nospinner");
    if(isWorld == false || installList.count() > 1)
    {
        cmd.append(" --oneshot");
    }

    if(ask)
    {
        cmd.append(" --ask");
    }
    installView->setText(cmd);
    installView->setTitle("Apps to Install...");
}

void BrowserWindow::uninstall(QString atom)
{
    if(clip == false)
    {
        return;
    }

    if(uninstallView == nullptr)
    {
        uninstallView = ui->tabWidget->createTab();

        History::State state;
        state.pos = QPoint(0, 0);
        state.target = "";
        state.title = "Apps to Uninstall...";
        uninstallView->history->appendHistory(state);
    }
    else
    {
        ui->tabWidget->setCurrentWidget(uninstallView);
    }

    if(uninstallList.contains(atom) == false)
    {
        uninstallList.append(atom);
    }
    ui->lineEdit->clear();
    uninstallView->browser()->clear();
    uninstallView->setIcon(":/img/clipboard.svg");
    QString cmd = "sudo emerge --unmerge";
    foreach(atom, uninstallList)
    {
        cmd.append(QString(" =%1").arg(atom));
    }
    cmd.append(" --nospinner --noreplace");
    if(ask)
    {
        cmd.append(" --ask");
    }
    uninstallView->setText(cmd);
    uninstallView->setTitle("Apps to Uninstall...");
}

BrowserWindow* BrowserWindow::openWindow()
{
    BrowserWindow *window = browser->createWindow();
    window->resize(width(), height());
    window->show();
    return window;
}

void BrowserWindow::reloadDatabase()
{
    setWorking(true);
    if(rescan == nullptr)
    {
        rescan = new RescanThread(this);
    }

    connect(rescan, SIGNAL(progress(int)), ui->searchProgress, SLOT(setValue(int)));
    connect(rescan, SIGNAL(finished()), this, SLOT(reloadDatabaseComplete()));

    currentView()->saveScrollPosition();
    currentView()->reloadingDatabase();
    currentView()->setStatus("Loading package database...");
    rescan->rescan();
}

void BrowserWindow::reloadDatabaseComplete()
{
    disconnect(rescan, SIGNAL(progress(int)), ui->searchProgress, SLOT(setValue(int)));
    disconnect(rescan, SIGNAL(finished()), this, SLOT(reloadDatabaseComplete()));
    workFinished();

    currentView()->reload(false);
    ui->lineEdit->setFocus();
}

void BrowserWindow::viewUpdates()
{
    QString s = "update:";

    CompositeView* view;
    for(int i = 0; i < ui->tabWidget->count(); i++)
    {
        view = ui->tabWidget->viewAt(i);
        if(view->url() == s)
        {
            ui->tabWidget->setCurrentIndex(i);
            ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
            ui->lineEdit->setText(s);
            return;
        }
    }

    // couldn't find an already open tab with available updates, so open a new tab instead:
    view = ui->tabWidget->createTab();
    view->navigateTo(s, true);
    view->setFocus();
    ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), view->icon());
    ui->lineEdit->setText(s);
}

void BrowserWindow::switchToTab(int index)
{
    if(ui->tabWidget->count() <= 0)
    {
        return;
    }

    if(index == 0)
    {
        // switch to last tab
        ui->tabWidget->setCurrentIndex(ui->tabWidget->count() - 1);
    }
    else if(index <= ui->tabWidget->count())
    {
        ui->tabWidget->setCurrentIndex(index - 1);
    }
    else
    {
        return;
    }

    QString s = ui->tabWidget->currentView()->url();
    ui->lineEdit->setText(s);
    ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
}

void BrowserWindow::closeEvent(QCloseEvent* event)
{
    if(browser->windows.count() == 1)
    {
        // if this is the last window remaining open, don't delete the window.
        // instead, we quit the application, which will save this window's tabs before
        // closing down the app.
        setVisible(false);
        event->ignore();
        qApp->quit();
        return;
    }

    if (ui->tabWidget->count() > 1)
    {
        int ret = QMessageBox::warning(this, tr("Confirm close"),
                                       tr("Are you sure you want to close the window ?\n"
                                          "There are %1 tabs open.").arg(ui->tabWidget->count()),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::No)
        {
            event->ignore();
            return;
        }
    }

    event->accept();
    deleteLater();
}

void BrowserWindow::keyPressEvent(QKeyEvent* event)
{
    int nextIndex;
    switch(event->key())
    {
        case Qt::Key_Backtab:
            nextIndex = ui->tabWidget->currentIndex();
            nextIndex--;
            if(nextIndex < 0)
            {
                nextIndex = ui->tabWidget->count() - 1;
            }
            ui->tabWidget->setCurrentIndex(nextIndex);
            event->accept();
            break;

        case Qt::Key_Tab:
            nextIndex = ui->tabWidget->currentIndex();
            nextIndex++;
            if(nextIndex >= ui->tabWidget->count())
            {
                nextIndex = 0;
            }
            ui->tabWidget->setCurrentIndex(nextIndex);
            event->accept();
            break;
    }
}


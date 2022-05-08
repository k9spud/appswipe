// Copyright (c) 2021-2022, K9spud LLC.
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
#include "k9portage.h"
#include "rescanthread.h"
#include "main.h"
#include "datastorage.h"
#include "browser.h"
#include "browserview.h"
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
            if(ui->tabWidget->currentView() == installView)
            {
                installList.clear();
                installView = nullptr;
            }
            else if(ui->tabWidget->currentView() == uninstallView)
            {
                uninstallList.clear();
                uninstallView = nullptr;
            }
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
    connect(ui->backButton, &QPushButton::clicked, this, &BrowserWindow::back);
    addAction(action);

    shortcuts.clear();
    shortcuts.append(QKeySequence::keyBindings(QKeySequence::Forward));
    action = new QAction(tr("&Forward"), this);
    action->setShortcuts(shortcuts);
    connect(action, &QAction::triggered, this, &BrowserWindow::forward);
    connect(ui->forwardButton, &QPushButton::clicked, this, &BrowserWindow::forward);
    addAction(action);

    action = new QAction(tr("&Find"), this);
    action->setShortcuts(QKeySequence::Find);
    connect(action, &QAction::triggered, this, [this]()
    {
        bool ok = false;
        QString search = QInputDialog::getText(this, tr("Find"),
                                               tr("Find:"), QLineEdit::Normal,
                                               lastSearch, &ok);
        if(ok && !search.isEmpty())
        {
            lastSearch = search;
            bool found = ui->tabWidget->currentView()->find(lastSearch);
            if(found == false)
            {
                ui->tabWidget->currentView()->setStatus(tr("\"%1\" not found.").arg(lastSearch));
            }
        }
    });
    addAction(action);

    action = new QAction(tr("Find &Next"), this);
    action->setShortcut(QKeySequence(Qt::Key_F3));
    connect(action, &QAction::triggered, [this]()
    {
        if(lastSearch.isEmpty() == false)
        {
            bool found  = ui->tabWidget->currentView()->find(lastSearch);
            if(found == false)
            {
                ui->tabWidget->currentView()->setStatus(tr("\"%1\" not found.").arg(lastSearch));
            }
        }
    });
    addAction(action);

    action = new QAction(tr("Find &Previous"), this);
    action->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(action, &QAction::triggered, [this]()
    {
        if(lastSearch.isEmpty() == false)
        {
            bool found = ui->tabWidget->currentView()->find(lastSearch, QTextDocument::FindBackward);
            if(found == false)
            {
                ui->tabWidget->currentView()->setStatus(tr("\"%1\" not found.").arg(lastSearch));
            }
        }
    });
    addAction(action);

    connect(ui->tabWidget, &TabWidget::titleChanged, this, [this](const QString& title)
    {
        if(title.isEmpty())
        {
            setWindowTitle(APP_NAME);
        }
        else
        {
            setWindowTitle(title + " - " APP_NAME);
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
    connect(action, &QAction::triggered, this, [this]()
    {
        QString cmd = "eselect news read";
        shell->externalTerm(cmd, "News");
    });
    menu->addAction(action);

    action = new QAction("Security Advisories", this);
    connect(action, &QAction::triggered, this, [this]()
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
    connect(action, &QAction::triggered, this, [this]()
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
    connect(action, &QAction::triggered, this, [this]()
    {
        qApp->quit();
    });
    menu->addAction(action);
    addAction(action);

    setWorking(false);
    setWindowTitle(APP_NAME);
    updateAskButton();
    updateClipButton();

    ui->lineEdit->setFocus();
}

BrowserWindow::~BrowserWindow()
{
    delete ui;
}

TabWidget* BrowserWindow::tabWidget()
{
    return ui->tabWidget;
}

BrowserView* BrowserWindow::currentView()
{
    return ui->tabWidget->currentView();
}

void BrowserWindow::back()
{
    ui->tabWidget->currentView()->back();
}

void BrowserWindow::forward()
{
    ui->tabWidget->currentView()->forward();
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
        ui->tabWidget->currentView()->reload();
        setWorking(false);
        ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
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
        qApp->processEvents();
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
    ui->tabWidget->createTab();
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
    if(clip)
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
        installView = ui->tabWidget->createBackgroundTab();
        installView->setIcon(":/img/clipboard.svg");
    }

    installList.append(atom);
    installView->clear();
    QString cmd = "sudo emerge";
    foreach(atom, installList)
    {
        cmd.append(QString(" =%1").arg(atom));
    }
    cmd.append(" --newuse --verbose --verbose-conflicts --nospinner");
    if(isWorld == false)
    {
        cmd.append(" --oneshot");
    }

    if(ask)
    {
        cmd.append(" --ask");
    }
    installView->setText(cmd);
}

void BrowserWindow::uninstall(QString atom)
{
    if(clip == false)
    {
        return;
    }

    if(uninstallView == nullptr)
    {
        uninstallView = ui->tabWidget->createBackgroundTab();
        uninstallView->setIcon(":/img/clipboard.svg");
    }

    uninstallList.append(atom);
    uninstallView->clear();
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
    ui->lineEdit->setText(s);

    BrowserView* view;
    for(int i = 0; i < ui->tabWidget->count(); i++)
    {
        view = ui->tabWidget->tabView(i);
        if(view->url() == s)
        {
            ui->tabWidget->setCurrentIndex(i);
            ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
            return;
        }
    }

    // couldn't find an already open tab with available updates, so open a new tab instead:
    ui->tabWidget->createTab();
    ui->tabWidget->currentView()->navigateTo(s, true);
    ui->tabWidget->setTabIcon(ui->tabWidget->currentIndex(), ui->tabWidget->currentView()->icon());
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

    event->accept();
    ui->tabWidget->closeAll();
    deleteLater();
}

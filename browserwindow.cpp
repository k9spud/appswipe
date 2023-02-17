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
#include "k9mimedata.h"

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
#include <QDrag>
#include <QDragEnterEvent>
#include <QTabBar>

BrowserWindow::BrowserWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::BrowserWindow)
{
    setFocusPolicy(Qt::ClickFocus);
    setAcceptDrops(true);

    ui->setupUi(this);

    ui->newTabButton->tabWidget = ui->tabWidget;

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

    action = new QAction(tr("&Close Tab"), this);
    action->setShortcuts(QKeySequence::Close);
    connect(action, &QAction::triggered, this, [this]()
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
            setWindowTitle(objectName());
        }
        else
        {
            setWindowTitle(QString("%1 - %2").arg(title, objectName()));
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
    menu->setStyleSheet("background-color: #303030;\nborder: 1px solid #000000;");

    action = new QAction(this);
    action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F));
    connect(action, &QAction::triggered, this, &BrowserWindow::on_menuButton_pressed);
    addAction(action);

    action = new QAction("News", this);
    connect(action, &QAction::triggered, this, []()
    {
        // might want to browse /var/db/repos/gentoo/metadata/news/ instead
        // might also check /var/lib/gentoo/news/news-gentoo.unread to know when news is available?
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

    menu->addSeparator();

    action = new QAction("Sync Repos", this);
    connect(action, &QAction::triggered, this, [this]()
    {
        // QString cmd = "sudo emaint --auto sync";
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

    menu->addSeparator();

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

    action = new QAction("View Updates", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    connect(action, &QAction::triggered, this, &BrowserWindow::viewUpdates);
    menu->addAction(action);
    addAction(action);

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

    menu->addSeparator();

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

    menu->addSeparator();

    action = new QAction("Exit", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(action, &QAction::triggered, this, []()
    {
        qApp->quit();
    });
    menu->addAction(action);
    addAction(action);

    // ------------------------------------------------------------------------
    tabsMenu = new QMenu("Tabs Menu", this);
    tabsMenu->setStyleSheet("background-color: #303030;\nborder: 1px solid #000000;");

    action = new QAction("&New Window", this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(action, &QAction::triggered, this, &BrowserWindow::openWindow);
    tabsMenu->addAction(action);
    addAction(action);

    inactiveWindows = new QMenu("Open", tabsMenu);
    inactiveWindows->setStyleSheet("background-color: #2D2D2D;");
    tabsMenu->addMenu(inactiveWindows);

    action = new QAction(tr("&Rename Window..."), this);
    action->setShortcut(QKeySequence(Qt::Key_F2));
    connect(action, &QAction::triggered, this, [this]()
    {
        bool ok;
        QString windowName = QInputDialog::getText(this, "Rename Window...", "Window title:", QLineEdit::Normal, objectName(), &ok);
        if(ok)
        {
            setObjectName(windowName);
            setWindowTitle(QString("%1 - %2").arg(currentView()->title(), windowName));
            browser->saveWindow(this);
        }
    });
    tabsMenu->addAction(action);
    addAction(action);

    action = new QAction(tr("Save A&ll"), this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(action, &QAction::triggered, browser, &Browser::saveAllWindows);
    tabsMenu->addAction(action);
    addAction(action);

    tabsMenu->addSeparator();

    action = new QAction(tr("&Discard Window"), this);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F4));
    connect(action, &QAction::triggered, this, [this]()
    {
        int ret = QMessageBox::warning(this, tr("Discard Window"),
                                       tr("Are you sure you want to discard \"%2?\"\n"
                                          "There are %1 tabs open.").arg(ui->tabWidget->count()).arg(objectName()),
                                       QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);

        if(ret == QMessageBox::Cancel)
        {
            return;
        }
        else if(ret == QMessageBox::Discard)
        {
            browser->deleteWindow(windowId);
            deleteLater();
        }
    });
    tabsMenu->addAction(action);
    addAction(action);
    // ------------------------------------------------------------------------

    setWorking(false);
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

void BrowserWindow::on_newTabButton_longPressed()
{
    inactiveWindows->clear();
    QVector<Browser::WindowHash> windows = browser->inactiveWindows();
    QAction* action;
    foreach(Browser::WindowHash w, windows)
    {
        action = inactiveWindows->addAction(w.title);
        connect(action, &QAction::triggered, this, [w]()
        {
            browser->loadWindow(w.windowId);
        });
    }

    QPoint point = mapToGlobal(ui->newTabButton->pos());
    point.setX(point.x());
    point.setY(point.y() + ui->newTabButton->height());
    tabsMenu->exec(point);
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
    setWindowTitle(QString("%1 v%2").arg(APP_NAME, APP_VERSION));
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

    if(ui->tabWidget->count() <= 1)
    {
        browser->deleteWindow(windowId);
    }
    else
    {
        browser->saveWindow(this, false);
    }

    event->accept();
    deleteLater();
}

void BrowserWindow::resizeEvent(QResizeEvent* event)
{
    if(windowState() == Qt::WindowNoState || windowState() == Qt::WindowActive)
    {
        QRect r;
        if(isWayland())
        {
            r = frameGeometry();
        }
        else
        {
            r = geometry();
        }

        unmaximizedSize = r.size();
    }

    QMainWindow::resizeEvent(event);
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

void BrowserWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const K9MimeData* mime = qobject_cast<const K9MimeData*>(event->mimeData());
    if(mime != nullptr)
    {
        event->acceptProposedAction();
    }
}

void BrowserWindow::dropEvent(QDropEvent* event)
{
    const K9MimeData* mime = qobject_cast<const K9MimeData*>(event->mimeData());

    if(mime != nullptr)
    {
        QObject* source = event->source();
        if(source == ui->tabWidget->tabBar() || source == ui->newTabButton)
        {
            event->acceptProposedAction();
            return;
        }

        disconnect(mime->view->history, nullptr, mime->sourceTabWidget->window, nullptr);
        disconnect(mime->view, nullptr, mime->sourceTabWidget, nullptr);

        int dropAt = ui->tabWidget->insertAfter + 1;
        int index = ui->tabWidget->insertTab(dropAt, mime->view, "");
        ui->tabWidget->setTabIcon(index, mime->view->icon());
        ui->tabWidget->setTabVisible(index, true);
        ui->tabWidget->setCurrentWidget(mime->view);

        ui->tabWidget->connectView(mime->view);

        if(mime->sourceTabWidget->count() == 0)
        {
            BrowserWindow* window = mime->sourceTabWidget->window;
            browser->deleteWindow(window->windowId);
            window->deleteLater();
        }
    }

    event->acceptProposedAction();
}


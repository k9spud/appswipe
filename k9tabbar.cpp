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

#include "k9tabbar.h"
#include "tabwidget.h"
#include "compositeview.h"
#include "globals.h"
#include "browser.h"
#include "browserwindow.h"
#include "k9mimedata.h"

#include <QEnterEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QToolButton>
#include <QAction>
#include <QLabel>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPushButton>
#include <QIcon>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QRect>

K9TabBar::K9TabBar(QWidget* parent) : QTabBar(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);

    moving = false;
    movingView = nullptr;

    label = new QLabel(parent);
    label->setVisible(false);
    label->setWordWrap(true);
    label->setStyleSheet(R"EOF(
background-color: rgba(57, 57, 57, 255);
border-top-right-radius: 10px;
border-bottom-right-radius: 10px;
padding-left: 1px;
)EOF");

    installEventFilter(this);
    setObjectName("K9TabBar");
}

K9TabBar::~K9TabBar()
{
    delete label;
}

// Disable Tool Tips from popping up, since we do our own title pop up boxes
// in mouseMoveEvent/enterEvent/leaveEvent
bool K9TabBar::eventFilter(QObject* object, QEvent* event)
{
    Q_UNUSED(object);

    if(event->type() == QEvent::ToolTip)
    {
        return true;
    }
    return false;
}

// This overrides the default behavior of mouse wheel switching between tabs and
// instead makes the mouse wheel scroll up and down the list of tabs without switching.
void K9TabBar::wheelEvent(QWheelEvent* event)
{
    int dy = event->angleDelta().y();
    if(dy > 0)
    {
        scrollUp();
    }
    else
    {
        scrollDown();
    }

    QPointF pos = event->position();
    int tab = tabAt(pos.toPoint());
    showTabLabel(tab);

    event->accept();
}

void K9TabBar::mousePressEvent(QMouseEvent* event)
{
    pressedPoint = event->pos();

    movingView = nullptr;
    if(event->button() == Qt::LeftButton)
    {
        TabWidget* tabWidget = qobject_cast<TabWidget*>(parent());
        if(tabWidget != nullptr)
        {
            movingView = tabWidget->viewAt(tabAt(pressedPoint));
        }
        moving = true;
        movingResist = true;
    }
    else
    {
        moving = false;
        movingResist = true;
    }

    QTabBar::mousePressEvent(event);
}

void K9TabBar::dragEnterEvent(QDragEnterEvent* event)
{
    const K9MimeData* mime = qobject_cast<const K9MimeData*>(event->mimeData());
    if(mime != nullptr)
    {
        event->acceptProposedAction();
    }
}

void K9TabBar::dropEvent(QDropEvent* event)
{
    const K9MimeData* mime = qobject_cast<const K9MimeData*>(event->mimeData());
    if(mime != nullptr)
    {
        TabWidget* tabWidget = qobject_cast<TabWidget*>(parent());
        int dropAt = tabAt(event->pos());
        if(dropAt == -1)
        {
            dropAt = tabWidget->insertAfter;
        }
        else
        {
            QRect rect = tabRect(dropAt);
            if(event->pos().y() >= (rect.y() + rect.height() / 2))
            {
                dropAt++;
            }
        }

        if(event->source() == this)
        {
            if(mime->sourceIndex < dropAt && dropAt > 0)
            {
                dropAt--;
            }
        }
        else
        {
            disconnect(mime->view->history, nullptr, tabWidget->window, nullptr);
            disconnect(mime->view, nullptr, mime->sourceTabWidget, nullptr);
        }

        int index = tabWidget->insertTab(dropAt, mime->view, "");
        tabWidget->setTabIcon(index, mime->view->icon());
        tabWidget->setTabVisible(index, true);
        tabWidget->setCurrentWidget(mime->view);

        if(event->source() != this)
        {
            tabWidget->connectView(mime->view);
        }

        if(mime->sourceTabWidget->count() == 0)
        {
            BrowserWindow* window = mime->sourceTabWidget->window;
            browser->deleteWindow(window->windowId);
            window->deleteLater();
        }
    }

    event->acceptProposedAction();
}

void K9TabBar::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();
    int tab = tabAt(pos);
    if(tab >= 0 && tab != labeledTab)
    {
        showTabLabel(tab);
    }

    if(moving)
    {
        QPoint delta = pos - pressedPoint;
        if(delta.x() > 30 || delta.x() < -30)
        {
            if(movingView != nullptr)
            {
                TabWidget* tabWidget = qobject_cast<TabWidget*>(parent());
                if(tabWidget != nullptr)
                {
                    moving = false;

                    QDrag* drag = new QDrag(this);
                    drag->setPixmap(movingView->icon().pixmap(32, 32));

                    K9MimeData *mimeData = new K9MimeData();
                    mimeData->setData("CompositeView", QByteArray("AppSwipe"));
                    mimeData->view = movingView;
                    mimeData->sourceIndex = tabWidget->indexOf(movingView);
                    mimeData->sourceTabWidget = tabWidget;
                    drag->setMimeData(mimeData);

                    Qt::DropAction action = drag->exec();

                    if(action == Qt::IgnoreAction)
                    {
                        BrowserWindow* window  = browser->createWindow();
                        TabWidget* newTabWidget = window->tabWidget();

                        disconnect(movingView->history, nullptr, tabWidget->window, nullptr);
                        disconnect(movingView, nullptr, tabWidget, nullptr);

                        int index = newTabWidget->addTab(movingView, "");
                        newTabWidget->setTabIcon(index, movingView->icon());
                        newTabWidget->setTabVisible(index, true);

                        newTabWidget->connectView(movingView);

                        int w, h;
                        w = tabWidget->window->width();
                        h = tabWidget->window->height();
                        window->resize(w, h);
                        window->show();
                    }

                    movingView = nullptr;
                    moving = false;
                }
            }
        }
        else if(movingResist == false || delta.y() > 18 || delta.y() < -18)
        {
            movingResist = false;
            QTabBar::mouseMoveEvent(event);
        }
    }
}

void K9TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton)
    {
        QPoint pos = event->pos();
        int tab = tabAt(pos);
        if(tab >= 0)
        {
            TabWidget* tabWidget = qobject_cast<TabWidget*>(parent());
            if(tabWidget != nullptr)
            {
                tabWidget->closeTab(tab);
                event->accept();
                qApp->processEvents();

                if(count() == 1)
                {
                    showTabLabel(-1);
                }
                else
                {
                    QPoint pos = event->pos();
                    int tab = tabAt(pos);
                    showTabLabel(tab);
                }
                return;
            }
        }
    }

    movingView = nullptr;
    moving = false;
    movingResist = true;
    QTabBar::mouseReleaseEvent(event);
}

void K9TabBar::showTabLabel(int tab)
{
    TabWidget* tabWidget = qobject_cast<TabWidget*>(parent());
    QString text;
    if(tabWidget != nullptr && tab >= 0 && tab < tabWidget->count())
    {
         CompositeView* view = tabWidget->viewAt(tab);
         if(view != nullptr)
         {
             text = view->title();
         }
    }

    if(tab < 0 || tab == currentIndex() || text.isEmpty())
    {
        label->setVisible(false);
        labeledTab = -1;
        return;
    }

    label->setText(text);
    QRect r = tabRect(tab);
    QFont font = label->font();
    QFontMetrics fm(font);
    QRect max(0, 0, parentWidget()->width() - r.width(), r.height());
    QRect rect = fm.boundingRect(max, Qt::TextWordWrap, label->text());

    label->setGeometry(width(), r.y(), rect.width() + 15, r.height());
    label->setVisible(true);
    labeledTab = tab;
}

void K9TabBar::enterEvent(QEvent* event)
{
    QEnterEvent* e = static_cast<QEnterEvent*>(event);
    if(e == nullptr)
    {
        qDebug() << event;
        return;
    }
    QPoint pos = e->pos();
    int tab = tabAt(pos);
    showTabLabel(tab);

    QTabBar::enterEvent(event);
}

void K9TabBar::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);

    showTabLabel(-1);
    QTabBar::leaveEvent(event);
}

void K9TabBar::scrollUp(int ticks)
{
    QToolButton* btn;
    foreach(QObject* o, children())
    {
        btn = qobject_cast<QToolButton*>(o);
        if(btn != nullptr)
        {
            if(btn->arrowType() == Qt::UpArrow)
            {
                for(int i = 0; i < ticks; i++)
                {
                    emit btn->clicked(true);
                }
                qApp->processEvents();
                break;
            }
        }
    }
}

void K9TabBar::scrollDown(int ticks)
{
    QToolButton* btn;
    foreach(QObject* o, children())
    {
        btn = qobject_cast<QToolButton*>(o);
        if(btn != nullptr)
        {
            if(btn->arrowType() == Qt::DownArrow)
            {
                for(int i = 0; i < ticks; i++)
                {
                    emit btn->clicked(true);
                }
                qApp->processEvents();
                break;
            }
        }
    }
}

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

#include "k9pushbutton.h"
#include "tabwidget.h"
#include "browserwindow.h"
#include "browser.h"
#include "compositeview.h"
#include "k9mimedata.h"
#include "globals.h"

#include <QMouseEvent>
#include <QDrag>

#define LONGPRESSMS 500

K9PushButton::K9PushButton(QWidget* parent) : QPushButton(parent)
{
    tabWidget = nullptr;
    connect(&timer, SIGNAL(timeout()), this, SLOT(longPressTimeout()));
}

void K9PushButton::longPressTimeout()
{
    timer.stop();
    emit longPressed();
}

void K9PushButton::mousePressEvent(QMouseEvent* event)
{
    if(tabWidget != nullptr)
    {
        pressedPoint = event->pos();

        if(event->button() == Qt::LeftButton)
        {
            moving = true;
        }
        else
        {
            moving = false;
            if(event->button() == Qt::RightButton)
            {
                emit longPressed();
                event->accept();
                return;
            }
        }
    }

    timer.start(LONGPRESSMS);
    QPushButton::mousePressEvent(event);
}

void K9PushButton::mouseMoveEvent(QMouseEvent* event)
{
    if(tabWidget != nullptr)
    {
        QPoint pos = event->pos();
        if(moving)
        {
            QPoint delta = pos - pressedPoint;
            if(delta.x() > 30 || delta.x() < -30)
            {
                if(tabWidget->currentView() != nullptr)
                {
                    moving = false;
                    timer.stop();
                    CompositeView* view = tabWidget->currentView();

                    QDrag* drag = new QDrag(this);
                    drag->setPixmap(view->icon().pixmap(32, 32));

                    K9MimeData *mimeData = new K9MimeData();
                    mimeData->setData("CompositeView", QByteArray("AppSwipe"));
                    mimeData->view = view;
                    mimeData->sourceIndex = tabWidget->indexOf(view);
                    mimeData->sourceTabWidget = tabWidget;
                    drag->setMimeData(mimeData);

                    Qt::DropAction action = drag->exec();

                    if(action == Qt::IgnoreAction)
                    {
                        if(tabWidget->count() > 1)
                        {
                            BrowserWindow* window  = browser->createWindow();
                            TabWidget* newTabWidget = window->tabWidget();

                            disconnect(view->history, nullptr, tabWidget->window, nullptr);
                            disconnect(view, nullptr, tabWidget, nullptr);

                            int index = newTabWidget->addTab(view, "");
                            newTabWidget->setTabIcon(index, view->icon());
                            newTabWidget->setTabVisible(index, true);

                            newTabWidget->connectView(view);

                            int w, h;
                            w = tabWidget->window->width();
                            h = tabWidget->window->height();
                            window->resize(w, h);
                            window->show();
                        }
                    }

                    moving = false;
                    event->accept();
                    return;
                }
            }
        }
    }

    QPushButton::mouseMoveEvent(event);
}

void K9PushButton::mouseReleaseEvent(QMouseEvent* event)
{
    moving = false;

    if(timer.isActive())
    {
        timer.stop();
        event->accept();

        switch(event->button())
        {
            case Qt::LeftButton:
                emit clicked();
                break;

            case Qt::RightButton:
                emit longPressed();
                break;

            default:
                break;
        }
    }

    QPushButton::mouseReleaseEvent(event);
}

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

#include "k9tabbar.h"
#include "tabwidget.h"

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

K9TabBar::K9TabBar(QWidget* parent) : QTabBar(parent)
{
    setMouseTracking(true);

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

void K9TabBar::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();
    int tab = tabAt(pos);
    if(tab >= 0 && tab != labeledTab)
    {
        showTabLabel(tab);
    }

    QTabBar::mouseMoveEvent(event);
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

    QTabBar::mouseReleaseEvent(event);
}

void K9TabBar::showTabLabel(int tab)
{
    QString text;
    if(tab < 0 || tab == currentIndex() || (text = tabToolTip(tab)).isEmpty())
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

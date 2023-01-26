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

#include <QFontMetrics>
#include <QFrame>

#define LONGPRESSMS 500

K9PushButton::K9PushButton(QWidget* parent) : QPushButton(parent)
{
    connect(&timer, SIGNAL(timeout()), this, SLOT(longPressTimeout()));
}

void K9PushButton::mousePressEvent(QMouseEvent* e)
{
    timer.start(LONGPRESSMS);
    e->accept();
}

void K9PushButton::mouseReleaseEvent(QMouseEvent* e)
{
    if(timer.isActive())
    {
        timer.stop();
        e->accept();

        switch(e->button())
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

}

void K9PushButton::longPressTimeout()
{
    timer.stop();
    emit longPressed();
}

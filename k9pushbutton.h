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

#ifndef K9PUSHBUTTON_H
#define K9PUSHBUTTON_H

#include <QPushButton>
#include <QMouseEvent>
#include <QTimer>

class TabWidget;
class K9PushButton : public QPushButton
{
    Q_OBJECT

    public:
        K9PushButton(QWidget *parent = nullptr);

        TabWidget* tabWidget;

    signals:
        void longPressed();

    protected:
        virtual void mousePressEvent(QMouseEvent* event) override;
        virtual void mouseReleaseEvent(QMouseEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent *event) override;
        bool moving;
        QPoint pressedPoint;

        QTimer timer;

    private slots:
        void longPressTimeout();
};

#endif // K9PUSHBUTTON_H

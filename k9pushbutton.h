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

#include <QtWidgets/QPushButton>
#include <QtGui/QMouseEvent>
#include <QTimer>

class K9PushButton : public QPushButton
{
    Q_OBJECT

    public:
        K9PushButton(QWidget *parent = Q_NULLPTR);

    signals:
        void clicked();
        void longPressed();

    protected:
        virtual void mousePressEvent(QMouseEvent* event) override;
        virtual void mouseReleaseEvent(QMouseEvent* event) override;

    private slots:
        void longPressTimeout();

    protected:
        QTimer timer;
};

#endif // K9PUSHBUTTON_H

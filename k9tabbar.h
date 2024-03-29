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

#ifndef K9TABBAR_H
#define K9TABBAR_H

#include <QTabBar>
#include <QEnterEvent>

class QLabel;
class QPushButton;
class QDrag;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class TabWidget;
class CompositeView;
class K9TabBar : public QTabBar
{
    Q_OBJECT
public:
    K9TabBar(QWidget *parent = nullptr);
    ~K9TabBar();

    void showTabLabel(int tab);
    void scrollUp(int ticks = 1);
    void scrollDown(int ticks = 1);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;

    virtual void enterEvent(QEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;

    QLabel* label;
    int labeledTab;

    bool moving;
    bool movingResist;
    CompositeView* movingView;
    QPoint pressedPoint;
};

#endif // K9TABBAR_H

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

#ifndef HISTORY_H
#define HISTORY_H

#include <QObject>
#include <QPoint>

class QMenu;
class QAction;
class History : public QObject
{
    Q_OBJECT
public:
    explicit History(QObject *parent = nullptr);
    ~History();

    enum WebAction {
        NoWebAction = - 1,
        Back,
        Forward,
        Stop,
        Reload
    };

    struct State
    {
        QString target; // url
        QString title;
        QPoint pos;     // scroll bars position
    };
    QVector<State> history;
    int currentHistory;
    State* currentState();
    QString currentTarget();
    QString currentTitle();

    QMenu* forwardMenu();
    QMenu* backMenu();

public slots:
    void appendHistory(const History::State& s);
    void updateState(const History::State& s);
    void back();
    void forward();
    void checkEnables(void);

signals:
    void enableChanged(History::WebAction action, bool enabled);
    void stateChanged(History::State s);

    void hovered(QString text);

private slots:
    void actionHovered();
    void actionTriggered();

private:
    QMenu* forwardStates;
    QMenu* backStates;
    QAction* prependMenu(QMenu* menu, int historyIndex);

};

#endif // HISTORY_H

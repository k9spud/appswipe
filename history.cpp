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

#include "history.h"

#include <QAction>
#include <QMenu>

History::History(QObject *parent)
    : QObject{parent}
{
    currentHistory = -1;
    forwardStates = nullptr;
    backStates = nullptr;
}

History::~History()
{
    if(forwardStates)
    {
        forwardStates->deleteLater();
    }

    if(backStates)
    {
        backStates->deleteLater();
    }
}

History::State* History::currentState()
{
    if(currentHistory >= 0 && currentHistory < history.count())
    {
        return &(history[currentHistory]);
    }

    return nullptr;
}

QString History::currentTarget()
{
    if(currentHistory < 0)
    {
        return "";
    }

    return history.at(currentHistory).target;
}

QString History::currentTitle()
{
    if(currentHistory < 0 || currentHistory >= history.count())
    {
        return "";
    }

    return history.at(currentHistory).title;
}

QMenu* History::forwardMenu()
{
    if(forwardStates == nullptr)
    {
        forwardStates = new QMenu();
        forwardStates->setStyleSheet(R"EOF(
    color: rgb(255, 255, 240);
    font: 13pt "Roboto";
    background-color: rgb(0, 0, 0);
    selection-color: rgb(255, 255, 240);
    selection-background-color: rgb(0, 100, 0);
    background-color: #393939;
)EOF");

        if(currentHistory >= 0 && currentHistory < history.count() - 1)
        {
            QAction* action;
            State state;
            for(int i = currentHistory + 1; i < history.count(); i++)
            {
                state = history.at(i);
                action = forwardStates->addAction(state.title);
                action->setStatusTip(state.target);
                action->setData(i);
                connect(action, &QAction::triggered, this, &History::actionTriggered);
                connect(action, &QAction::hovered, this, &History::actionHovered);
            }
        }
    }
    return forwardStates;
}

QMenu* History::backMenu()
{
    if(backStates == nullptr)
    {
        backStates = new QMenu();
        backStates->setStyleSheet(R"EOF(
    color: rgb(255, 255, 240);
    font: 13pt "Roboto";
    background-color: rgb(0, 0, 0);
    selection-color: rgb(255, 255, 240);
    selection-background-color: rgb(0, 100, 0);
    background-color: #393939;
)EOF");

        if(currentHistory > 0 && currentHistory < history.count())
        {
            QAction* action;
            State state;
            for(int i = currentHistory - 1; i >= 0; i--)
            {
                state = history.at(i);
                action = backStates->addAction(state.title);
                action->setStatusTip(state.target);
                action->setData(i);
                connect(action, &QAction::triggered, this, &History::actionTriggered);
                connect(action, &QAction::hovered, this, &History::actionHovered);
            }
        }
    }
    return backStates;
}

void History::appendHistory(const State& newState)
{
    if(currentHistory >= 0 && currentHistory < history.count() - 1)
    {
        history.remove(currentHistory + 1, (history.count() - currentHistory) - 1);
        if(forwardStates)
        {
            forwardStates->clear();
        }
    }

    if(currentHistory >= 0 && currentHistory < history.count())
    {
        history[currentHistory].pos = newState.pos;

        if(backStates)
        {
            prependMenu(backStates, currentHistory);
        }
    }

    history.append(newState);
    currentHistory++;
    checkEnables();
}

void History::updateState(const State& s)
{
    if(currentHistory >= 0 && currentHistory < history.count())
    {
        history.replace(currentHistory, s);
    }
}

void History::back()
{
    if(currentHistory > 0)
    {
        if(backStates)
        {
            backStates->removeAction(backStates->actions().first());
        }

        if(forwardStates)
        {
            prependMenu(forwardStates, currentHistory);
        }

        emit stateChanged(history.at(currentHistory - 1));
        currentHistory--;
        checkEnables();
    }
}

void History::forward()
{
    if(currentHistory < (history.count() - 1))
    {
        if(forwardStates)
        {
            forwardStates->removeAction(forwardStates->actions().first());
        }

        if(backStates)
        {
            prependMenu(backStates, currentHistory);
        }

        emit stateChanged(history.at(currentHistory + 1));
        currentHistory++;
        checkEnables();
    }
}

void History::checkEnables()
{
    if(currentHistory < (history.count() - 1))
    {
        emit enableChanged(Forward, true);
    }
    else
    {
        emit enableChanged(Forward, false);
    }

    if(currentHistory > 0)
    {
        emit enableChanged(Back, true);
    }
    else
    {
        emit enableChanged(Back, false);
    }
}

void History::actionHovered()
{
    QAction* action = qobject_cast<QAction*>(sender());

    if(action != nullptr)
    {
        emit hovered(action->statusTip());
    }
}

void History::actionTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    int index = action->data().toInt();

    if(forwardStates != nullptr)
    {
        forwardStates->deleteLater();
        forwardStates = nullptr;
    }

    if(backStates != nullptr)
    {
        backStates->deleteLater();
        backStates = nullptr;
    }

    emit stateChanged(history.at(index));
    currentHistory = index;
    checkEnables();
}

QAction* History::prependMenu(QMenu* menu, int historyIndex)
{
    State state = history.at(historyIndex);
    QAction* action;
    if(menu->actions().count() == 0)
    {
        action = menu->addAction(state.title);
    }
    else
    {
        action = new QAction(state.title, menu);
        menu->insertAction(menu->actions().first(), action);
    }

    action->setStatusTip(state.target);
    action->setData(historyIndex);
    connect(action, &QAction::triggered, this, &History::actionTriggered);
    connect(action, &QAction::hovered, this, &History::actionHovered);

    return action;
}

// Copyright (c) 2021, K9spud LLC.
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

#include "k9lineedit.h"

#include <QClipboard>
#include <QKeyEvent>
#include <QApplication>

K9LineEdit::K9LineEdit(QWidget *parent) : QLineEdit(parent)
{

}

void K9LineEdit::keyPressEvent(QKeyEvent* event)
{
    if(event->modifiers() == (Qt::ControlModifier + Qt::ShiftModifier))
    {
        switch(event->key())
        {
            case Qt::Key_C:
                if(hasSelectedText())
                {
                    QClipboard* clip = qApp->clipboard();
                    clip->setText(selectedText());
                }
                else if(text().length())
                {
                    QClipboard* clip = qApp->clipboard();
                    clip->setText(text());
                }
                return;
        }
    }

    QLineEdit::keyPressEvent(event);
}

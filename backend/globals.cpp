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

#include "globals.h"
#include "k9portage.h"

#include <QProcessEnvironment>

bool showProgress = false;
K9Portage* portage = nullptr;
QTextStream output(stdout);
QTextStream error(stderr);

void progress(int i)
{
    if(showProgress)
    {
        error << "progress " << i << Qt::endl;
    }
}

QString fileSize(qint64 size)
{
    QString s;
    if(size > 1024 * 1024 * 1024)
    {
        s = QString::number((size * 100) / (1024 * 1024 * 1024));
        s.insert(s.length() - 2, ".");
        s.append(" GB");
    }
    else if(size > 1024 * 1024)
    {
        s = QString::number((size * 10) / (1024 * 1024));
        s.insert(s.length() - 1, ".");
        s.append(" MB");
    }
    else if(size > 1024)
    {
        s = QString::number((size * 10) / 1024);
        s.insert(s.length() - 1, ".");
        s.append(" KB");
    }
    else
    {
        s = QString::number(size) + " bytes";
    }

    return s;
}

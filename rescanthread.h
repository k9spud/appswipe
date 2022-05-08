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

#ifndef RESCANTHREAD_H
#define RESCANTHREAD_H

#include <QThread>
#include <QHash>
#include <QVariant>
#include <QRegularExpression>

class RescanThread : public QThread
{
    Q_OBJECT
public:
    explicit RescanThread(QObject *parent = nullptr);
    bool abort;

    void rescan();

    
signals:
    void progress(int percent);

protected:
    void run() override;
};

#endif // RESCANTHREAD_H

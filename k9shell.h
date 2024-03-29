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

#ifndef K9SHELL_H
#define K9SHELL_H

#include <QObject>
#include <QProcess>

extern class K9Shell* shell;

class K9Shell : public QProcess
{
    Q_OBJECT
public:
    explicit K9Shell(QObject *parent = nullptr);

    void findBackend();
    void findTransport();
    void findBzip2();
    QString backend;
    QString transport;
    QString doas;
    QString bzip2;

public slots:
    void externalBrowser(QString url);
    void externalTerm(QString cmd);
    void externalTerm(QString cmd, QString title, bool waitSuccessful = true);
    void externalFileManager(QString url);

signals:

protected:

};

#endif // K9SHELL_H

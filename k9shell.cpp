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

#include "k9shell.h"

#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>

class K9Shell* shell = nullptr;

K9Shell::K9Shell(QObject *parent) : QProcess(parent)
{
    tmp = nullptr;
}

void K9Shell::externalBrowser(QString url)
{
    QStringList args;
    args << "--launch" << "WebBrowser";
    args << url;
    startDetached("/usr/bin/exo-open", args);
}

void K9Shell::externalTerm(QString cmd)
{
    if(tmp != nullptr)
    {
        delete tmp;
    }
    tmp = new QTemporaryFile();
    if(tmp->open() == false)
    {
        qDebug() << "Couldn't open temporary file.";
        return;
    }

    QTextStream out(tmp);
    out << "echo \"" << cmd << "\"\n";
    out << cmd << "\n";
    out << "read -rsn1 -p\"Press any key to close (exit code: $?)\n\";echo\n";
    out.flush();
    tmp->close();

    QStringList term;
    term << "-T" << cmd << "-x" << "/bin/bash" << tmp->fileName();
    startDetached("/usr/bin/xfce4-terminal", term);
}

void K9Shell::externalFileManager(QString url)
{
    QStringList args;
    args << "--launch" << "FileManager";
    args << url;
    startDetached("/usr/bin/exo-open", args);
}

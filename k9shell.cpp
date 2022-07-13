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

#include "k9shell.h"

#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>
#include <QApplication>

K9Shell* shell = nullptr;

K9Shell::K9Shell(QObject *parent) : QProcess(parent)
{
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
    externalTerm(cmd, cmd);
}

void K9Shell::externalTerm(QString script, QString title, bool waitSuccessful)
{
    QTemporaryFile* tmp = new QTemporaryFile();
    if(tmp->open() == false)
    {
        qDebug() << "Couldn't open temporary file.";
        delete tmp;
        return;
    }

    QTextStream out(tmp);
    QString escaped;

    QStringList cmds = script.split("\n");
    QString cmd;
    for(int i = 0; i < cmds.count(); i++)
    {
        cmd = cmds.at(i);
        escaped = cmd;
        if(escaped.contains("\""))
        {
            escaped.replace("\"", "\\\"");
        }

        if(cmd.contains("RET_CODE=$?") == false &&
           cmd.contains(QString("%1").arg(qApp->applicationFilePath())) == false)
        {
            out << "echo \"" << escaped << "\"\n";
        }
        out << cmd << "\n";
    }

    if(cmd.endsWith("| less") == false)
    {
        if(script.contains("RET_CODE=$?") == false)
        {
            out << "RET_CODE=$?\n";
        }

        if(waitSuccessful == false)
        {
            out << "if [[ $RET_CODE -eq 0 ]];\n";
            out << "then\n";
            out << "  echo \"Success!\"\n";
            out << "else\n";
            out << "  echo \"Press 'q' to close (exit code: $RET_CODE)\"\n";
            out << "  while true; do\n";
            out << "    read -n1 -rs\n";
            out << "    [[ $REPLY == 'q' || $REPLY == 'Q' ]] && break\n";
            out << "  done\n";
            out << "fi\n";
        }
        else
        {
            out << "echo \"Press 'q' to close (exit code: $RET_CODE)\"\n";
            out << "while true; do\n";
            out << "  read -n1 -rs\n";
            out << "  [[ $REPLY == 'q' || $REPLY == 'Q' ]] && break\n";
            out << "done\n";
        }
    }
    out.flush();
    tmp->close();

    QStringList term;
//    term << "-T" << cmd << "-x" << "/bin/bash" << tmp->fileName();
//    startDetached("/usr/bin/xfce4-terminal", term);
    term << "-T" << title << "-e" << "/bin/bash" << tmp->fileName();
    startDetached("/usr/bin/lxterminal", term);
}

void K9Shell::externalFileManager(QString url)
{
    QStringList args;
    args << "--launch" << "FileManager";
    args << url;
    startDetached("/usr/bin/exo-open", args);
}

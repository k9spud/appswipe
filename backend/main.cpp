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

#include "main.h"
#include "globals.h"
#include "k9portage.h"
#include "datastorage.h"
#include "rescanthread.h"

#include <signal.h>
#include <QCoreApplication>
#include <QProcessEnvironment>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("K9spud LLC");
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication a(argc, argv);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if(env.contains("RET_CODE"))
    {
        // abort if emerge process failed to actually do anything
        int retCode = env.value("RET_CODE").toInt();
        if(retCode != 0)
        {
            return retCode;
        }
    }

    portage = new K9Portage();
    portage->setRepoFolder("/var/db/repos/");

    ds = new DataStorage();
    ds->openDatabase();

    bool emerged = false;
    bool synced = false;
    bool reload = false;
    qint64 pid = -1;
    QString atom;
    QString reloadAtom;

    for (int i = 1; i < argc; ++i)
    {
        if(qstrcmp(argv[i], "-emerged") == 0)
        {
            i++;
            if(i < argc)
            {
                atom = argv[i];
                emerged = true;
            }
            continue;
        }

        if(qstrcmp(argv[i], "-reload") == 0)
        {
            i++;
            if(i < argc)
            {
                reloadAtom = argv[i];
                reload = true;
            }
            continue;
        }

        if(qstrcmp(argv[i], "-progress") == 0)
        {
            showProgress = true;
            continue;
        }

        if(qstrcmp(argv[i], "-synced") == 0)
        {
            synced = true;
            continue;
        }

        if(qstrcmp(argv[i], "-pid") == 0)
        {
            i++;
            if(i < argc)
            {
                pid = atoll(argv[i]);
            }
            continue;
        }
    }

    if(synced)
    {
        if(rescan == nullptr)
        {
            rescan = new RescanThread(nullptr);
        }

        rescan->abort = false;
        rescan->reloadDatabase();
    }
    else
    {
        if(emerged)
        {
            portage->emergedApp(atom);
        }

        if(reload)
        {
            rescan->reloadApp(reloadAtom);
        }
    }

    if(pid != -1)
    {
        kill(pid, SIGHUP);
    }
    return 0;
}

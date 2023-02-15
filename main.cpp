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

#include "main.h"
#include "globals.h"
#include "k9portage.h"
#include "browser.h"
#include "browserwindow.h"
#include "datastorage.h"
#include "k9shell.h"
#include "rescanthread.h"

#include <signal.h>
#include <QApplication>
#include <QUrl>

QCoreApplication* createApplication(int &argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if(qstrcmp(argv[i], "-emerged") == 0 ||
           qstrcmp(argv[i], "-synced") == 0 ||
           qstrcmp(argv[i], "-pid") == 0)
        {
            return new QCoreApplication(argc, argv);
        }
    }
    return new QApplication(argc, argv);
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setOrganizationName("K9spud LLC");
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QScopedPointer<QCoreApplication> app(createApplication(argc, argv));

    portage = new K9Portage();
    portage->setRepoFolder("/var/db/repos/");

    ds = new DataStorage();
    ds->openDatabase();

    QApplication* a = qobject_cast<QApplication*>(app.data());
    if(a)
    {
        a->setWindowIcon(QIcon(QStringLiteral(":/img/appicon.svg")));
    }
    else
    {
        bool emerged = false;
        bool synced = false;
        qint64 pid = -1;
        QString atom;

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
        else if(emerged)
        {
            portage->reloadApp(atom);
        }

        if(pid != -1)
        {
            kill(pid, SIGHUP);
        }
        return 0;
    }

    shell = new K9Shell();

    browser = new Browser();
    BrowserWindow* window = nullptr;

    browser->restoreWindows();
    if(browser->windows.count() == 0)
    {
        window = browser->createWindow();
        window->show();
    }
    else
    {
        window = browser->windows.first();
    }

    if(window != nullptr)
    {
        if(ds->emptyDatabase)
        {
            window->reloadDatabase();
            ds->emptyDatabase = false;
        }
    }

    int retCode = app->exec();
    delete browser;
    browser = nullptr;

    return retCode;
}

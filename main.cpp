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

#include "main.h"
#include "globals.h"
#include "k9portage.h"
#include "browser.h"
#include "browserwindow.h"
#include "tabwidget.h"
#include "browserview.h"
#include "datastorage.h"

#include <QApplication>
#include <QUrl>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setOrganizationName("K9spud LLC");
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(QStringLiteral(":/img/appicon.svg")));

    portage = new K9Portage();
    portage->setRepoFolder("/var/db/repos/");

    ds = new DataStorage();
    ds->openDatabase();

    Browser browser;
    if(browser.windows.count() == 0)
    {
        BrowserWindow* window = browser.createWindow();
        window->show();
        if(ds->emptyDatabase)
        {
            window->reloadDatabase();
            ds->emptyDatabase = false;
        }
/*
        bool startAbout = true;
        QStringList args = QCoreApplication::arguments();
        QString arg;
        for(int i = 1; i < args.count(); i++)
        {
            arg = args.at(i);
            if(arg.startsWith('-') == false)
            {
                window->currentView()->navigateTo(arg);
                startAbout = false;
                break;
            }
        }

        if(startAbout)
        {
            window->currentView()->about();
        }*/
    }

    return a.exec();
}

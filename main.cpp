// Copyright (c) 2021-2025, K9spud LLC.
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

#include <QApplication>
#include <QUrl>

int main(int argc, char *argv[])
{
#if QT_VERSION < 0x060000
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QCoreApplication::setOrganizationName("K9spud LLC");
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QApplication app(argc, argv);

    portage = new K9Portage();

    ds = new DataStorage();
    ds->openDatabase();

    app.setWindowIcon(QIcon(QStringLiteral(":/img/appicon.svg")));
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

    int retCode = app.exec();
    delete browser;
    browser = nullptr;

    return retCode;
}

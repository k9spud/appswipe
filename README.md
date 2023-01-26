
App Swipe
=========
Copyright (c) 2021-2023, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://user-images.githubusercontent.com/39664841/139709601-35b9a8e7-e431-4631-98de-572ddafe5242.png)

This program was written with Qt 5.15.7, ``dev-db/sqlite-3.40.1``, 
``sys-apps/portage-3.0.41-r2``, ``app-portage/portage-utils-0.94.3``, 
``dev-libs/glib-2.74.3-r3``, and ``lxde-base/lxterminal-0.4.0``.

Installation
============

There is an ebuild for App Swipe in my 
[k9spud-overlay](https://github.com/k9spud/k9spud-overlay). 
Alternatively, there is an App Swipe ebuild in the 
[Project GURU](https://wiki.gentoo.org/wiki/Project:GURU) 
repository, although theirs might lag a bit since I don't personally maintain 
their ebuild.

To add the k9spud-overlay to your system, do the following:

```console
sudo su -
cd /var/db/repos
git clone https://www.github.com/k9spud/k9spud-overlay.git k9spud
nano -w /etc/portage/repos.conf/k9spud-overlay.conf
```

Insert the following text into the k9spud-overlay.conf file:

```console
[k9spud]

location = /var/db/repos/k9spud
sync-type = git
sync-uri = https://www.github.com/k9spud/k9spud-overlay.git
priority = 200
auto-sync = yes
clone-depth = 1
sync-depth = 1
sync-gitclone-extra-opts = --single-branch --branch main
```

After that, use the following to install App Swipe:

```console
emerge app-portage/appswipe
```

If all went well, you can now run the `appswipe` program from your normal 
user-level account.

Keyboard Shortcuts
==================

`CTRL-T` Opens a new tab.

`CTRL-W` Closes the current tab.

`CTRL-N` Opens a new window.

`CTRL-Q` Saves each window's geometry and open tabs, then quits the application.

`CTRL-TAB` Displays the next tab.

`CTRL-SHIFT-TAB` Displays the prior tab.

`CTRL-1` through `CTRL-9` Displays the first, second, etc tab.

`CTRL-0` Displays the very last tab.

`CTRL-backtick` Displays the `update:` upgradable package list tab. Opens one if none previously opened.

`CTRL-L` Jumps to the URL input text box.

`SHIFT-Return` Navigates to the first result of an application search.

`F5` Refreshes the browser view (soft reload)

`CTRL-R` Reloads the SQLite database for this app and refreshes view (hard reload).

`ALT-Back Arrow Key` Displays the previous page in browser history.

`ALT-Forward Arrow Key` Displays the next page in browser history.

`CTRL-F` Searches browser view text.

`F3` Repeats last search, moving forward.

`SHIFT-F3` Repeats last search, going backwards.

`Escape` Reserved for stopping long running browser view operations (probably inoperable at the moment, since we don't have any such slow operations, right?)

`ALT-F` Opens the app menu.

What's New in v1.1.34?
======================

Added a workaround for popup menus showing up at the wrong location on screen
when using Wayland.

Now ignores differences in the "subslot" number when generating an "update:" 
list, to better match what Portage does.

"List files owned" now displays the package's file list in-app instead of 
launching an external terminal window. This is still kind of clunky though,
because Qt's QTextEdit widget chokes if you get a list of files that is too
long. For now, we simply truncate the list if it exceeds 2,000 entries.

Using an URL like "file:///" allows you to browse your file system 
like old web browsers used to do for FTP sites. Added image viewing mode.

Added right click menu option to "Build binary package" for those times when
you want to build a package, but not install it immediately. 

No longer uses XFCE's "exo" launcher and instead uses the "gio" launcher from
``dev-libs/glib`` for launching an external browser. This eliminates the 
dependency on XFCE, which itself was depending on ``dev-libs/glib`` under
the hood anyway.

Fixed a bug where closing the clipboard tab could cause the program to 
segfault. The clipboard contents is now saved and restored when shutting down
the application. 

Long pressing Forward/Back buttons now pops up a menu of page history for
the given direction.

License
=======

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or (at your 
option) any later version. 

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
General Public License for more details. 

You should have received a copy of the GNU General Public License 
along with this program; if not, write to the Free Software Foundation, 
Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

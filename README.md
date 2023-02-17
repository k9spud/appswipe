
App Swipe
=========
Copyright (c) 2021-2023, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://user-images.githubusercontent.com/39664841/139709601-35b9a8e7-e431-4631-98de-572ddafe5242.png)

This program was written with Qt 5.15.8, ``sys-apps/portage-3.0.43-r1``, 
``app-portage/portage-utils-0.94.3``, ``app-portage/gentoolkit-0.6.1-r3``, 
``dev-libs/glib-2.74.4``, and ``lxde-base/lxterminal-0.4.0``.

Installation
============

There is an ebuild for App Swipe in my 
[k9spud-overlay](https://github.com/k9spud/k9spud-overlay). 
Alternatively, there is an App Swipe ebuild in the 
[Project GURU](https://wiki.gentoo.org/wiki/Project:GURU) 
repository, although theirs may lag behind since I don't personally maintain 
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

`CTRL-SHIFT-S` Saves each window's geometry and open tabs, without quitting.

`F2` Allows you to rename the current window.

`CTRL-ALT-F4` Permanently discards the current window.

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

What's New in v1.1.41?
======================

New tabs/window management. Now you can drag and drop a tab from one window 
to another, just like modern web browsers. 

Right click or long press the "Open New Tab" button to access the window 
management menu. You can rename each browser window to whatever name you'd 
like to remember it by. Close the window, and later re-open that window 
with all of it's multiple tabs intact. Saved windows are still remembered 
even when you exit and re-start the app. Window maximized state now restores
properly, including keeping the correct unmaximized window size intact when 
the window is maximized.

The new tabs/window management is good for categorizing groups of related 
apps together for looking at later. It helps reduce memory usage and
screen clutter when closing windows that aren't important right now. 
When you're finallycompletely done with a window and it's tabs, use 
"Discard Window" to permanently close the window.

Note: if you simply close a window (rather than "Discard"), the window is 
saved for later use, but only if it has multiple tabs open. If you close a 
window that has only one tab open, it is assumed that what you really mean 
is that you don't care about that one tab anymore, so the window is 
automatically permanently discarded.

USE flags are now hyperlinked when viewing an app. Clicking a USE flag
link brings up descriptions for the USE flag and other apps that accept
a USE flag by the same name. 

An alternative "Who depends on this?" reverse dependencies command has been
added. This one uses the `qdepends` command from `portage-utils` instead
of `equery depends.` `qdepends` seems to be blazingly fast compared to
`equery,` and it usually provides equally good info. Right click in the 
whitespace (not on an hyperlink) when viewing an app to access this 
alternate reverse dependencies lookup.

Added new "Install & rebuild reverse dependencies" command to app
install right click menu. This option can sometimes come in handy 
when `emerge` refuses to install an app because of dependency conflicts.

I fetch new apps to try out on my Raspberry Pi 4 often, and new apps almost 
never keyword "arm64" off the bat. When "Fetch source" is used, App Swipe 
now automatically adds the selected app to 
`/etc/portage/package.accept_keywords/appswipe.tmp`. This is kind of a 
hack -- but it's just too convenient for making fetch source "just go" no 
matter what emerge thinks.

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

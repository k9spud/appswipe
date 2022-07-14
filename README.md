App Swipe
=========
Copyright (c) 2021-2022, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://user-images.githubusercontent.com/39664841/139709601-35b9a8e7-e431-4631-98de-572ddafe5242.png)

This program was written with Qt 5.15.3, ``dev-db/sqlite-3.38.5``, ``sys-apps/portage-3.0.30-r3``, 
``app-portage/portage-utils-0.93.3``, ``xfce-base/exo-4.17.1``, and ``lxde-base/lxterminal-0.4.0``.

Installation
============

There is an ebuild for App Swipe in my [k9spud-overlay](https://github.com/k9spud/k9spud-overlay). 
Alternatively, there is an App Swipe ebuild in the [Project GURU](https://wiki.gentoo.org/wiki/Project:GURU) 
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

If all went well, you can now run the `appswipe` program from your normal user-level account.

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

What's New in v1.1.21?
======================

**TLDR: Faster and smoother.**

On Wayland, the app was saving window sizes slightly smaller than actual, 
because the window frame wasn't being included. This resulted in windows 
getting slightly smaller each time the app was closed and re-opened, which
was annoying.

Now hides Forward/Back toolbar buttons when not actually available for use. 
This leaves more room for the URL input text edit box.

Fixed some bugs with saving the current scroll position of the browser view
when hitting Forward/Back. 

Fixed a bug where the Run/Build time dependencies list was sometimes off by 
one.

Hitting the `Refresh` button in the browser view now does a "soft reload" of 
the ebuild that is much faster by avoiding reparsing ebuilds that haven't
likely actually changed and avoiding the really slow apply masks operation. 
You can still get the old "hard reload" behavior by holding down `CTRL` and 
clicking `Refresh` or simply pressing `CTRL-R` by itself. 

Added "Are you sure?" prompt when attempting to close a window with more 
than one tab.

Improved availability of `CTRL-Tab` and `CTRL-SHIFT-Tab` hotkeys when input 
focus is on the wrong spot.

Improved the color of toolbar buttons when clicking on them. Shrunk the
browser scroll bars so that they are right up against the edge of the window 
instead of having a one pixel gap.

When adding or removing a tab, Qt does some bizarre things with 
re-positioning the display of available tabs if there are enough tabs to 
scroll off the display. Added code to try to counteract Qt's brain-dead 
re-positioning of the tab bar scroll position. It's not perfect, but it is
about as close as I can get using the limited access Qt provides
for manipulating the tab bar's scroll position.

Starting an ebuild install now invokes the portage-utils `qlop` tool to
(sometimes) provide a rough build time estimate.

Now handles UNIX SIGHUP and SIGTERM signals. SIGTERM causes the app to 
immediately save all the window sizes and tabs to the underlying SQLite 
database (normally done only when exiting the app). 

SIGHUP causes each open browser tab to refresh it's display of the 
underlying data. 

Added some rudimentary inter-process communication using these signals so 
that after an ebuild has been successfully installed/uninstalled,
the GUI will automatically refresh to reflect the now current state. Added
`-emerged`, `-synced`, and `-pid` command line options for GUI-less updating
of the underlying SQLite database and notifying the GUI to refresh.

Unfortunately, this feature does not (yet) attempt to do any scanning of 
`/var/log/emerge` to pick up on any *additional* ebuilds that got sucked into 
being upgraded or installed by dependency. So you still need to manually 
`Reload Database` from time to time to keep AppSwipe's internal SQLite 
database up-to-date with the actual system state.

Fixed bug in ebuild parsing. String assignments with embedded escaped quotation
marks might work now.

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

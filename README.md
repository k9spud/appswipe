App Swipe
=========
Copyright (c) 2021-2022, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://user-images.githubusercontent.com/39664841/139709601-35b9a8e7-e431-4631-98de-572ddafe5242.png)

This program was written with Qt 5.15.3, ``dev-db/sqlite-3.38.2``, ``sys-apps/portage-3.0.30-r3``, 
``app-portage/portage-utils-0.93.3``, ``xfce-base/exo-4.17.1``, and ``lxde-base/lxterminal-0.4.0``.

Installation
============

There is an ebuild for App Swipe in my [k9spud-overlay](https://github.com/k9spud/k9spud-overlay). To add the k9spud-overlay to your system, do the following:

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

If all goes well, you can then run the `appswipe` program from your normal user-level account.

What's New in v1.1.14?
======================

Added a new "View Updates" menu option. This attempts to build a suggested
list of ebuilds that you will likely want to upgrade. It attempts to stay 
within stable releases if the currently installed ebuild is stable -- this is
something I find frustratingly lacking in portage. App Swipe does not attempt 
to suggest upgrades beyond the currently installed slot, which portage 
does do occasionally (with great success). I don't know how portage manages 
that, so I can't mimic it yet.

This feature does not attempt to do any dependency resolving like portage 
does, so it can give you a much faster result than doing an 
"emerge --update @world" operation. But of course, this makes the App Swipe
updates view rather blind to new packages that you may need to install 
before upgrading particular ebuilds with new dependencies.

Added more support for detecting whether an ebuild has been masked by one
of the very many ways portage has for masking them.

Fixed some bugs in the way the app determines if an ebuild's status on the
current CPU architecture (aka "keyword") is stable, testing, or unknown. 
The SQLite database schema has been updated to include a STATUS column so 
that querying this information is easier to do.

Fixed some bugs with moving the mouse over top of links making the status 
bar and cursor shape wonky in certain edge cases.

Fixed a bug where the result count when doing a search came out wrong
sometimes.

Added SHIFT-Return keyboard shortcut in the URL editing box so that App Swipe
jumps straight into the first app of the search results list (akin to Google's 
Feeling Lucky button).

CTRL-L keyboard shortcut now selects all text in the URL editing box if 
you're already focused inside that box.

Added CTRL-1, CTRL-2, CTRL-3, etc keyboard shortcuts for quickly switching to
the first, second, third, etc respective browsing tab. CTRL-0 jumps down to 
the very last tab (regardless of whatever tab number that happens to be).

Added ALT-F keyboard shortcut for opening the app menu.

Opening a new tab now pops up under the currently shown tab instead of adding
itself all the way down at the end of all the existing tabs.

Instead of showing the raw USE/IUSE flags, we now only display the IUSE flags
that did not get used in the installed build. This makes it easier to find
what USE flags you could be using, particularly for ebuilds that have a lot 
of flags available.

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

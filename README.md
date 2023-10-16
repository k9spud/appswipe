
App Swipe
=========
Copyright (c) 2021-2023, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://github.com/k9spud/appswipe/assets/39664841/754e807d-4e57-457e-8d54-554d38e8a070)

What's New in v1.1.57?
======================
Dependencies are now displayed for packages that haven't been installed 
yet, thanks to the information in repo metadata caches. Even better,
App Swipe now does lookups on each dependency for an uninstalled package to
determine what other packages you need to install. These unsatisfied 
dependencies show up highlighted in red. 

![Unmet Dependency Highlighting](https://github.com/k9spud/appswipe/assets/39664841/765e80c3-f2b7-4f15-b461-315bab07dea3)

Added support for using `doas` instead of only supporting `sudo`. Removed
`sudo` from the PDEPENDS list. If neither is installed, App Swipe will
fall back to simply `echo` the commands it can't execute with elevated 
privileges.

Previously, the code for updating the internal SQLite database after 
installing/upgrading a package did not update the total installed disk
space used by the package. Now we update the database with the size of the
installed package.

Auto-keyword unmasking has been refactored. We no longer sudo a shell script
to append to /etc/portage/package.accept_keyword/appswipe.tmp. Instead, we 
require the user to set up such a file and give it write permissions for 
so that sudo/doas is not required. App Swipe now does some checking to try 
to avoid adding accept_keyword entries for packages that have already been 
keyword unmasked so that this file doesn't get bloated as quickly.

Right click "List files owned" has been removed. Instead, a link on the app 
view page for the installed size can be used. This reduces the number 
of clicks needed to get to the installed files list.

Help Get the Word Out
=====================

If you like this program, help us reach more people by clicking the `Star`
button at the top of our [Github](https://github.com/k9spud/appswipe) page.
It costs nothing and it helps App Swipe get listed higher up in Github 
search results when people are looking for this sort of thing.

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

If all went well, you can now run the `appswipe` program.

Root Privileges
================
Do not run App Swipe as 'root.' For searching and browsing your local 
repository files, App Swipe never uses any elevated privileges. Only
when you attempt to make system wide changes (install/upgrade/uninstall/etc)
are elevated permissions necessary.

In order to execute `emerge` and other tools with elevated permissions, 
App Swipe will either use the `doas` or `sudo` command (whichever is 
installed on your system). If you've already got one of these configured to
let you run commands as root, you probably don't need to do any additional 
configuration. However, if you want a more fine grained configuration, the 
following may be useful:

Example `doas` configuration
----------------------------
When using [doas](https://wiki.gentoo.org/wiki/Doas), you could add the 
following to `/etc/doas.conf` (substitute the username you run App Swipe 
under for USER):

```console
permit USER cmd /usr/bin/emerge
permit USER cmd /usr/bin/ebuild
permit USER cmd /usr/sbin/dispatch-conf
```

Example `sudo` configuration
----------------------------
When using [sudo](https://wiki.gentoo.org/wiki/Sudo), you could add the 
following using the `visudo` command (substitute the username you 
run App Swipe under for USER):

```console
USER localhost = /usr/bin/emerge
USER localhost = /usr/bin/ebuild
USER localhost = /usr/sbin/dispatch-conf ""
```

Using App Swipe
===============

The first thing App Swipe does is scrape up all the scattered data about
installed packages from `/var/db/pkg` and packages available in repos 
at `/var/db/repos`. This data is inserted into a 
[SQLite](https://www.sqlite.org/) database that is stored at
`$HOME/.AppSwipe/AppSwipe.db`. 

From that point on, you can do searches and browse your portage repos
at blazing speed, thanks to the underlying SQLite database. As you install, 
upgrade, etc, App Swipe attempts to keep the SQLite database up-to-date, 
but this is currently not always perfect. If an emerge operation pulls in 
additional dependencies, for example, App Swipe currently has no way of 
knowing about these extra changes, so you may have to manually trigger 
a `Reload Database` operation from time to time, to bring App Swipe's 
internal database back up to matching what's really in your system at 
the moment.

Using the `View Updates` page
-----------------------------

The `View Updates` page features a list of all potentionally upgradable
packages that are currently installed on your system. The `Upgrade` link 
on this page lets you emerge all the upgradable packages that App Swipe 
shows here. This is similar to `Update World`, except App Swipe doesn't burn 
a bunch of CPU cycles determining which package dependencies descend from 
your @world list like emerge does. Instead, it lists any package 
already installed on your system that might be upgradable to a newer version.

The `View Updates` URL input box supports filters. Let's say you only want 
the subset of upgradable packages that contain the word `qt` for example. 
The URL `update:qt` will let you see only those packages, and the `Fetch` 
or `Upgrade` links on this filtered page will be limited to only fetching 
or upgrading the `qt` subset as well. 

Negative filters are supported. If you wanted to upgrade Qt packages, 
but want to avoid `qtwebengine` for now (because that one is *huge* and 
takes *forever*), you could simply use the URL 
`update:qt -qtwebengine`:

![Filtering updates](https://github.com/k9spud/appswipe/assets/39664841/71df9665-d329-4db0-b6e1-d3b8238a0662)

Automatic Keyword Unmasking
---------------------------

App Swipe can automatically keyword unmask packages when you attempt to 
fetch or install a "testing" or "unsupported" (non-keyworded) release.

To enable this feature, create an accept_keywords file with 
writable permissions for your username like this:

```console
cd /etc/portage/package.accept_keywords
touch appswipe.tmp
chown USER:USER appswipe.tmp
chmod 0644 appswipe.tmp
```
This feature may have security implications; it's use is optional. 
If you don't provide user write permission to the file 
`/etc/portage/package.accept_keywords/appswipe.tmp`, App Swipe will
silently skip doing automatic keyword unmasking.

Keyboard Shortcuts
==================

`CTRL-T` Opens a new tab.

`CTRL-W` Closes the current tab. Middle clicking on a tab's icon is another 
way to close a tab.

`CTRL-N` Opens a new window.

`CTRL-Q` Saves each window's geometry and open tabs, then quits the application.

`CTRL-SHIFT-S` Saves each window's geometry and open tabs, without quitting.

`F2` Allows you to rename the current window.

`CTRL-F4` Permanently discards the current window.

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

Common Problems
===============

When I right click and "Reinstall from source," it just reinstalls the 
previously built binary?
----------------------------------------------------------------------

This can be caused by having "FEATURES=${FEATURES} getbinpkg" in your
`/etc/portage/make.conf` file. App Swipe can't override FEATURES
from the command line.

The suggested workaround is to remove `getbinpkg` from the `FEATURES` 
variable, and instead add `--getbinpkg=y` to the `EMERGE_DEFAULT_OPTS`
variable. This has the same effect, except now App Swipe can override 
the use of binary packages when doing a "Reinstall from source" 
operation.

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

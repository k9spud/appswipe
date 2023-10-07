
App Swipe
=========
Copyright (c) 2021-2023, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

![Screenshot](https://github.com/k9spud/appswipe/assets/39664841/754e807d-4e57-457e-8d54-554d38e8a070)

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

If all went well, you can now run the `appswipe` program from your normal 
user-level account.

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

What's New in v1.1.52?
======================

Split off database reload code into a separate command line program, 
`appswipebackend`. Going forward, more and more code should be moved
into the backend so that only the bare essential code needed to keep the
GUI displayed is kept in resident memory.

Database loading code now supports capturing data from the repo's 
`metadata/md5-cache/` instead of trying to parse the data out of .ebuild 
files. This solves a lot of problems with slot numbers that were previously
showing bad data because the .ebuild files often do a lot of scripting
with regards to declaring a slot number. App Swipe will still continue to
fall back to parsing .ebuild files if the repo does not have metadata cache.

Now supports displaying bzip2 compressed files. 
Now supports displaying text files with markdown formatting.

Now does a better job of sorting packages by version number. Previously, 
version numbers containing alpha, beta, pre, rc (basically, any sort of 
"pre-release") could end up sorting out as being newer than the final 
release.

Fixed a bug in applying masks to the right versions when using the `<=` 
operator. 

Now supports packages with excessive tuples in the version string
(such as `net-ftp/ftp-0.17.34.0.2.5.1`). The SQL database schema now supports
up to 10 tuples maximum.

Available USE flags (IUSE) now shows the latest available set from 
the latest release in the repo rather than showing what the available USE
flags were for that package at install. This seems more useful, as it may let
you know more about what's really available out there. 

Found some packages where the same IUSE flag is listed multiple times - we 
now remove duplicates so App Swipe's display is less cluttered.

We now filter out USE flags that aren't in the package's available IUSE flags
list. This cleans up the display of extraneous USE flags that seem more like
internal portage flags than actual options for the user.

Hard reload (CTRL-R) when viewing an app page no longer blindly reloads all 
mask files like it used to. Instead, we only execute SQL updates
for the masks that contain this app's atom string. This makes hard 
reload work a lot faster while most likely still providing the same results.

Pressing Return while a link is highlighted in the browser view now triggers
navigating to the linked page. This keyboard shortcut comes in handy after 
doing a CTRL-F search for a particular package.

What's New in v1.1.48?
======================

Added support for laptops that have no right mouse button. Now you can left 
click and hold to trigger a "right click." 

`View Updates` now filters out live ebuilds with version numbers
9999, 99999, 999999, 9999999, 99999999, or 99999999 (just to be sure).

The `View Updates` screen now features an `Upgrade` button to let you emerge
all the upgradable packages that App Swipe sees. This is similar to
`Update World`, except App Swipe doesn't burn a bunch of CPU cycles 
determining which package dependencies descend from your @world list like
emerge does. Instead, we just list any package already installed on your 
system that could be upgraded to a newer version.

The `View Updates` URL input box now supports adding filters.
Let's say you only want the subset of upgradable packages
that contain the word `qt` for example. The URL `update:qt` will let you see
only those packages, and the `Fetch` or `Upgrade` buttons on this page will 
limit itself to only fetching or upgrading the `qt` subset as well. 

You can also apply negative filters. Let's say you want to upgrade Qt 
packages, but you want to avoid `qtwebengine` for now (because that one is 
*huge* and takes *forever*). You could simply use the URL 
`update:qt -qtwebengine`

![Filtering updates](https://github.com/k9spud/appswipe/assets/39664841/71df9665-d329-4db0-b6e1-d3b8238a0662)

`Update System` and `Update World` now add the `--changed-use` emerge flag. 
This flag makes emerge pick up on packages that need re-building due to
any new USE flag changes made to your portage configuration files.

We now trigger an automatic App Swipe database reload at the end of successful 
`emerge --update` operations.

Fixed a bug where the app could crash when saving state if there exists
a newly opened window that has no tabs.

Fixed a bug that made it impossible to view an app info page where the app name
has what might be a version number (but actually isn't). For example 
`media-fonts/font-bh-100dpi` would previously show a blank page when 
trying to bring it up.

Added code for removing /tmp/AppSwipe.XXX temporary file after it has been 
used.

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

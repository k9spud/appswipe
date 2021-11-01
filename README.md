App Swipe
=========
Copyright (c) 2021, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

The program was written with Qt 5.15.2, Sqlite 3.34.0, Portage 3.0.14, 
Portage Utils 0.90.1, exo 4.16.0, and Xfce4 Terminal 0.8.10.

What's New in v1.1.0?
======================

Now requires ``lxde-base/lxterminal`` instead of xfce4-terminal. Mainly
because memory usage is lower and it seems just as functional.

Now should work correctly on architectures beyond arm64. I've tested
it on AMD64, but many others should work fine too.

Terminal windows now require pressing the specific key ``q`` before
closing. Previously, any key would do, but that caused accidental window
closes for me. Spawned terminal windows now display better titlebar text.

Much improved app display. Now shows dependencies with clickable links
so you can easily research the underlying required packages.

CTRL-SHIFT+C now copies selected text to the clipboard. This just makes for 
less fussiness when copy and pasting to terminal windows, where CTRL-C 
doesn't do clipboard operations. 

App Swipe now tries to avoid adding packages to the @world file when
upgrading an existing installed package. Now it should only get added
automatically if you're installing a new package.

Browser window has been improved to allow click and drag scrolling of the
window from areas of whitespace. Allows supports kinetic scroll swiping.
Bug with links failing to navigate when there exists selected text is
fixed. Added a feature where the title line of the app can be clicked to
copy the atom to the clipboard.

Hamburger menu now pops up within the window instead of potentially going 
off-screen if the app window is close to the edge of the screen.

Added depclean action to the hamburger menu.

Added "--newuse" option to emerge commandline whenever installing a
package so that portage will pick up any new USE flags that might cause
packages to need rebuilding.

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

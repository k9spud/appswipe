App Swipe
=========
Copyright (c) 2021, K9spud LLC

This is an application for browsing your local Portage repository files.
Easily manage your system's applications from a point and click user interface
rather than typing out long command lines for installing, uninstalling, and 
upgrading apps.

The program was written with Qt 5.15.2, Sqlite 3.34.0, Portage 3.0.14, 
Portage Utils 0.90.1, exo 4.16.0, and Xfce4 Terminal 0.8.10.

What's New in v1.0.30?
======================

Viewing an application now shows more useful details, such as whether
the app is a member of the @world set, what CFLAGS were used to build it,
what the USE flags were, etc. 

The list of package versions should be sorted a little better now. 

Some support for displaying masked status has been implemented (may still 
have some bugs/unsupported mask filters, and doesn't traverse repo profile 
masks yet).

Added new right click menu option for "Reinstall from Source," which will 
force a rebuild of the selected package from source code rather than 
reinstall from a previously built binary package. 

Moving Forward and Back through history now retains the proper scroll 
position.

Upgrading from Prior Releases
=============================

After compiling and starting this new version, you should hit the "Reload 
Database" option from the top right menu (hit the apple with worm icon).
Otherwise, you won't get all the mask/world, etc display improvements.

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

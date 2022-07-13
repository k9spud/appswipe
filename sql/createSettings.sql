-- Copyright (c) 2021-2022, K9spud LLC.
--
-- This program is free software; you can redistribute it and/or
-- modify it under the terms of the GNU General Public License
-- as published by the Free Software Foundation; either version 2
-- of the License, or (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

create table if not exists META (
    SCHEMAVERSION integer,
    UUID text
);
insert into META(SCHEMAVERSION) SELECT 2 WHERE NOT EXISTS(SELECT 0 FROM META);

create table if not exists WINDOW (
    WINDOWID integer primary key,
    X integer,
    Y integer,
    W integer,
    H integer
);

create table if not exists TAB (
    TABID integer primary key,
    WINDOWID integer,
    URL text,
    TITLE text,
    SCROLLX integer,
    SCROLLY integer,
    CURRENTPAGE integer,
    ICON text
);

create table if not exists CATEGORY (
    CATEGORYID integer primary key autoincrement,
    CATEGORY text
);

create table if not exists REPO (
    REPOID integer primary key autoincrement,
    REPO text,
    LOCATION text
);

create table if not exists PACKAGE (
    PACKAGEID integer primary key autoincrement,
    CATEGORYID integer,
    REPOID integer,
    PACKAGE text,
    DESCRIPTION text,
    HOMEPAGE text,
    VERSION text,
    V1 int,
    V2 int,
    V3 int,
    V4 int,
    V5 int,
    V6 int,
    SLOT text,
    LICENSE text,
    INSTALLED integer,
    FROMBINARY integer,
    OBSOLETED integer,
    MASKED integer,
    DOWNLOADSIZE integer,
    KEYWORDS text,
    IUSE text,
    PUBLISHED integer,
    STATUS integer
);

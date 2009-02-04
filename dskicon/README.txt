dskicon - utility to create icons from Macintosh disks
Copyright (C) 2009 Jesus A. Alvarez, zydeco@namedfork.net

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

dskicon creates a png icon from a Macintosh disk image

To decide which icon to use, it searches in this order:
 * Custom Volume Icon (HFS, System 7 or newer)
 * Icon of single app on disk (if there's only one, excluding the System Folder)
 * Icon of app with name similar to volume name (if there are more apps)
 * Icon of app with name in volume comment (only on <2MB images)

Other features:
 * MFS and HFS disk images are supported with our without a 84 byte header.
 * icns resources are not supported, separate resources are used

Libraries used:
 * libmfs for accessing MFS disks
 * libhfs for accessing HFS disks (www.mars.org/home/rob/proj/hfs)
 * libres for accessing resource files

Additions to libhfs
 * Support old style disk images with a 84-byte header
 * Finder comment IDs in hfsdirent structure

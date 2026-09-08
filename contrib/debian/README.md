
Debian
====================
This directory contains files used to package xcoind/xcoin-qt
for Debian-based Linux systems. If you compile xcoind/xcoin-qt yourself, there are some useful files here.

## raven: URI support ##


xcoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install xcoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your xcoin-qt binary to `/usr/bin`
and the `../../share/pixmaps/raven128.png` to `/usr/share/pixmaps`

xcoin-qt.protocol (KDE)


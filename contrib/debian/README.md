
Debian
====================
This directory contains files used to package axed/axe-qt
for Debian-based Linux systems. If you compile axed/axe-qt yourself, there are some useful files here.

## axe: URI support ##


axe-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install axe-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your axe-qt binary to `/usr/bin`
and the `../../share/pixmaps/axe128.png` to `/usr/share/pixmaps`

axe-qt.protocol (KDE)


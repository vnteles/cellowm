# Cello - A floating window manager smooth as a cello
[![GitHub release](https://img.shields.io/badge/release-v0.1.2-blue.svg)](https://github.com/vnteles/cellowm/releases)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Description
Cello is a floating window manager for X11.

Cello is intended to be a lightweight and customizable window manager. It uses a client, called celloc, to comunicate with the wm, using a socket that allows you to write your own softwares to talk with the wm (such as voice assistents), not depending of celloc.

### important: this project is under development, not recommended to use it as your main window manager

## Todo
some functionalities I want to implement to cello (not necessary on this order)
- [x] Basic WM functionalities
- [x] Basic EWMH integration
- [x] Fullscreen windows
- [x] Basic bar/dock support - tested polybar, tint2, latte, plank(doesn't display softwares launched) and yabar(crash sometimes)
- [x] Temporary message parser
- [ ] Focus mode
- [ ] Multi-monitor support
- [ ] Reserve space for bars/docks
- [ ] New message parser
- [ ] Fix resize teleports
- [ ] Integrations with QT (set theme, icon theme, fonts, ...)
- [ ] EWMH integration
- [ ] Reparent windows to frame

## Building
### Dependencies
+ xcb
+ xcb-cursor
+ xcb-util-wm

### Building the sources
Make sure to have the dependencies above, the run:
``` sh
# Build the sources with
$ make
# And to install
$ make install
```

## Known bugs
- reload crash the wm
- resizing isn't natural
##### In case of new bugs, please, write an issue. pull requests are welcome too 😁

## Thanks
- sowm
- bswm
- 2bwm
- i3wm
- goomwwm

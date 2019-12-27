AM_CFLAGS = -Wall -std=c11 -O2 -I$(top_srcdir)/src/include
AM_LDFLAGS = -lxcb -lxcb-cursor -lxcb-ewmh -lxcb-icccm -lxcb-keysyms -lpthread

bin_PROGRAMS = cellowm
cellowm_SOURCES = cello.c confparse.c cursor.c ewmh.c handlers.c list.c main.c utils.c window.c xcb.c
AM_CFLAGS = -W -std=c11 -Wall -I$(top_srcdir)/src/include -lxcb -lxcb-cursor -lxcb-ewmh -lxcb-icccm -lxcb-keysyms -lpthread

prog_CFLAGS = $(AM_CFLAGS)

bin_PROGRAMS = cellowm
cellowm_SOURCES = cello.c confparse.c cursor.c ewmh.c handlers.c list.c main.c utils.c window.c xcb.c
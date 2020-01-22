#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "utils.h"


struct window_list;
struct window window;

struct geometry {
    int16_t x, y;
    uint16_t w, h;

    uint8_t depth;
};

struct window {
    xcb_window_t id;
    xcb_window_t frame;

    /*window geometry*/
    struct geometry geom;
    /*original window geometry, so we can go back from maximized windows with no problems*/
    struct geometry orig;

    xcb_drawable_t handlebar;

    /*store the state of the current struct window*/
    uint32_t state_mask;
    /*temporary state mask*/
    uint32_t tmp_state_mask;

    /*window desktop*/
    uint32_t d;

    /*desktop list*/
    struct window_list * dlist;

    /*window list*/
    struct window_list * wlist;
};


struct window_list {
    struct window_list * prev;
    struct window_list * next;

    struct window * window;
};

union action {
    /*command*/
    char ** com;
    /*config constant*/
    const uint32_t i;
};

#define MOD_HEADER \
    const unsigned int mod_mask; \
    void (*function)(const union action); \
    uint32_t win_mask; \
    const union action action; \

struct button {
    const uint8_t button;
    MOD_HEADER;
};

struct key {
    const xcb_keysym_t key;
    MOD_HEADER;
};


struct config {
    uint32_t inner_border;
    long inner_border_color;

    uint32_t outer_border;
    long outer_border_color;

    uint8_t border;

    struct key * keys;
    struct button * buttons;

    uint32_t focus_gap;

    uint8_t config_ok;
};

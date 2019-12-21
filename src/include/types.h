#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "utils.h"

struct list {
    struct list * prev;
    struct list * next;

    char * gdata;
};

struct window {
    xcb_drawable_t id;
    uint8_t depth;

    int16_t x, y;
    uint16_t w, h;

    xcb_drawable_t deco;
    uint32_t deco_mask;

    /*window desktop*/
    uint8_t d;
    /*desktop list*/
    struct list * dlist;

    /*window list*/
    struct list * wlist;
};

union param {
    const char ** com;
    const uint32_t i;
};

#define MOD_HEADER \
    const unsigned int mod_mask; \
    void (*function)(const union param *); \
    uint32_t win_mask; \
    const union param param; \

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

    uint8_t config_ok;
};

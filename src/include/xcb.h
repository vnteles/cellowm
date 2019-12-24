#pragma once

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "utils.h"


#define WINDOW_MIN_WIDTH 30
#define WINDOW_MIN_HEIGHT 20

#define ROOT_EVENT_MASK \
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT    |\
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY      |\
    XCB_EVENT_MASK_STRUCTURE_NOTIFY         |\
    XCB_EVENT_MASK_PROPERTY_CHANGE          |\
    XCB_EVENT_MASK_BUTTON_PRESS

xcb_screen_t * xcb_get_root_screen(xcb_connection_t * con, int scr);
void xcb_grab_buttons(xcb_window_t win);

void xcb_unfocus();
void xcb_focus_on_pointer();
void xcb_focus_window(struct window * w);

struct window * xcb_get_focused_window();

void xcb_raise_window(xcb_window_t win);
void xcb_raise_focused_window();

void xcb_move_window(struct window * w, int16_t x, int16_t y);
void xcb_move_focused_window(int16_t x, int16_t y);

void xcb_resize_window(struct window * w, uint16_t width, uint16_t height);
void xcb_resize_focused_window(uint16_t width, uint16_t height);
xcb_keycode_t * xcb_get_keycode_from_keysym(xcb_keysym_t keysym);
xcb_keysym_t xcb_get_keysym_from_keycode(xcb_keycode_t keycode);

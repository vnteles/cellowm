#define NO_KEYS
#include "xcb.h"

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "cello.h"
#include "types.h"
#include "ewmh.h"
#include "config.h"
#include "utils.h"
#include "list.h"
#include "log.h"

static struct window * focused;

#define each_screen ( ; it.rem; xcb_screen_next(&it) )
xcb_screen_t * xcb_get_root_screen(xcb_connection_t * con, int scr) {
    xcb_screen_iterator_t it = xcb_setup_roots_iterator( xcb_get_setup(con) );

    /*if the screen number == 0, return it*/
    for each_screen if ( !(scr--) ) return it.data;
    /*else, root screen not finded*/
    return NULL;
}
#undef each_screen

void xcb_grab_buttons(xcb_window_t win) {
    int len = sizeof(buttons)/sizeof(*buttons);
    int i;
    for (i = 0; i < len; i++) {
        xcb_grab_button(
            conn, false, win,
            XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC, root_screen->root,
            XCB_NONE, buttons[i].button,
            buttons[i].mod_mask
        );
    }
}

void xcb_unfocus() {
    /*already unfocused*/
    if (!focused || focused->id == root_screen->root)
        return;

    focused = NULL;
}

void xcb_focus_on_pointer() {
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);

    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, root_screen->root,
        ewmh->_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW,
        32, 1, &(xcb_window_t){0}
    );

    focused = NULL;

    xcb_flush(conn);
    return;
}

void xcb_focus_window(struct window * w) {
    /*if for some reason win be null*/
    if (!w) xcb_focus_on_pointer();
    else if (w->id == root_screen->root)    return;
    else {
        xcb_change_property(
            conn, XCB_PROP_MODE_REPLACE, w->id,
            ewmh->_NET_WM_STATE, ewmh->_NET_WM_STATE,
            32, 2, (uint32_t[]){ XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE }
        );
        xcb_set_input_focus(
            conn, XCB_INPUT_FOCUS_POINTER_ROOT,
            w->id, XCB_CURRENT_TIME
        );
        xcb_change_property(
            conn, XCB_PROP_MODE_REPLACE, root_screen->root,
            ewmh->_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1, &w->id
        );

        xcb_grab_buttons(w->id);
        focused = w;
        xcb_flush(conn);
    }
}

struct window * xcb_get_focused_window() {
    return focused;
}

void xcb_raise_window(xcb_window_t win) {
    if (win == root_screen->root) return;
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){ XCB_STACK_MODE_ABOVE });
    xcb_flush(conn);
}

void xcb_raise_focused_window() {
    xcb_raise_window(focused->id);
}

void xcb_move_window(struct window * w, int16_t x, int16_t y){
    if (w->id == root_screen->root || !w) return;
    /*don't move maximized windows*/
    if (w->state_mask & CELLO_STATE_MAXIMIZE || w->state_mask & CELLO_STATE_MONOCLE)
        return;

    w->geom.x = x;
    w->geom.y = y;

    xcb_configure_window(
        conn, w->id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
        (uint32_t[]){ x, y }
    );

    xcb_flush(conn);
}

void xcb_move_focused_window(int16_t x, int16_t y) {
    if (!focused) return;


    xcb_move_window(focused, x, y);
}

/*required verify the window proportions before calling this*/
void xcb_resize_window(struct window * w, uint16_t width, uint16_t height) {
    if (w->id == root_screen->root || !w) return;
    
    if (w->state_mask & CELLO_STATE_MAXIMIZE || w->state_mask & CELLO_STATE_MONOCLE)
        return;

    uint16_t mask = 0;
    uint32_t values[2] = {XCB_NONE, XCB_NONE};
    unsigned short i = 0;

    if ( (width > WINDOW_MIN_WIDTH) ) {
        mask |= XCB_CONFIG_WINDOW_WIDTH;
        values[i++] = width;
    }
    if ( (height > WINDOW_MIN_HEIGHT) ) {
        mask |= XCB_CONFIG_WINDOW_HEIGHT;
        values[i++] = height;
    }


    // xcb_configure_window(conn, w->frame, mask,values);
    xcb_configure_window(conn, w->id, mask,values);

    cello_decorate_window(w);
    xcb_flush(conn);
}

void xcb_resize_focused_window(uint16_t width, uint16_t height) {
    if (!focused) return;

    focused->geom.w = width;
    focused->geom.h = height;

    xcb_resize_window(focused, width, height);
}

xcb_keycode_t * xcb_get_keycode_from_keysym(xcb_keysym_t keysym) {
    xcb_key_symbols_t * keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        return NULL;

    xcb_keycode_t * keycode = xcb_key_symbols_get_keycode(keysyms, keysym);

    xcb_key_symbols_free(keysyms);
    return keycode;
}

xcb_keysym_t xcb_get_keysym_from_keycode(xcb_keycode_t keycode) {
    xcb_key_symbols_t * keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        return 0;

    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);

    xcb_key_symbols_free(keysyms);
    return keysym;
}

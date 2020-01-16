#include "events.h"

#include <stdio.h>

#include "xcb.h"
#include "cello.h"
#include "config.h"
#include "ewmh.h"
#include "log.h"

#define each_button \
    (unsigned int i = 0; i < (sizeof(buttons) / sizeof(*buttons)); i++)

static void on_key_press(xcb_generic_event_t * event) {
    xcb_key_press_event_t* e = (xcb_key_press_event_t*)event;

    xcb_keysym_t keysym = xcb_get_keysym_from_keycode(e->detail);

    uint16_t i;
    unsigned int keys_len = sizeof(keys) / sizeof(*keys);
    for (i = 0; i < keys_len; i++) {
        if (keys[i].function && keysym == keys[i].key) {
            if (keys[i].mod_mask == e->state) {
                keys[i].function(&keys[i].param);
                break;
            }
        }
    }
}

static void on_button_press(xcb_generic_event_t * event) {
    // puts("button");
    xcb_button_press_event_t* e = (xcb_button_press_event_t*)event;

    // cycle thought the button list
    for each_button {

        if (
            // ignore if the button hasn't any function or the button pressed isn't the same of the event
            (!buttons[i].function || buttons[i].button != e->detail) ||
            // or if the mask is different of the event's
            (buttons[i].mod_mask != e->state)
        ) continue;


        // ignore if the action is root-only and the event window isn't the root window
        if ((buttons[i].win_mask & ROOT_ONLY) && (e->event != e->root) ) continue;


        // now we can properly grab the action
        buttons[i].function(&buttons[i].param);
    }
}

static void on_enter_notify(xcb_generic_event_t * event) {
    // puts("enter");
    xcb_enter_notify_event_t* e = (xcb_enter_notify_event_t*)event;

    if (e->mode != XCB_NOTIFY_MODE_NORMAL && e->mode != XCB_NOTIFY_MODE_UNGRAB)
        return;

    struct window* focused;
    struct window* w;

    focused = xcb_get_focused_window();
    if (focused && e->event == focused->id) return;

    /*don't focus on non mapped windows*/
    if (!(w = find_window_by_id(e->event))) return;

    xcb_focus_window(w);
    xcb_flush(conn);
}
static void on_configure_notify(xcb_generic_event_t * event) {
    NLOG("Configure Notify event received");
    // only when set up xrandr
}

static void on_configure_request(xcb_generic_event_t * event) {
    xcb_configure_request_event_t* e = (xcb_configure_request_event_t*)event;

    uint32_t values[7];
    uint16_t mask = 0;
    uint8_t i = 0;

#define CLONE_MASK(MASK, CONFIG) \
    if (e->value_mask & MASK) {    \
        mask |= MASK;                \
        values[i++] = e->CONFIG;     \
    }

    CLONE_MASK(XCB_CONFIG_WINDOW_X, x);
    CLONE_MASK(XCB_CONFIG_WINDOW_Y, y);
    CLONE_MASK(XCB_CONFIG_WINDOW_WIDTH, width);
    CLONE_MASK(XCB_CONFIG_WINDOW_HEIGHT, height);
    CLONE_MASK(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width);
    CLONE_MASK(XCB_CONFIG_WINDOW_SIBLING, sibling);
    CLONE_MASK(XCB_CONFIG_WINDOW_STACK_MODE, stack_mode);

#undef CLONE_MASK
    DLOG("Window 0x%08x configured with geometry %dx%d at [%d, %d]",
       e->window, e->width, e->height, e->x, e->y);

    xcb_configure_window(conn, e->window, mask, values);
    xcb_flush(conn);
}

static void on_destroy_notify(xcb_generic_event_t * event) {
    // EFN(on_destroy_notify);

    xcb_destroy_notify_event_t* e = (xcb_destroy_notify_event_t*)event;
    VARDUMP(e->window);

    struct window* w;
    if ((w = find_window_by_id(e->window))) {
        cello_destroy_window(w);
    }
}

static void on_map_request(xcb_generic_event_t * event) {
    // EFN(on_map_request);

    xcb_map_request_event_t* e = (xcb_map_request_event_t*)event;
    struct window* win;
    // skip if windows in another desktop
    if (find_window_by_id(e->window)) return;

    // configure window
    if (!(win = window_configure_new(e->window))) return;

    win->d = cello_get_current_desktop();
    xcb_map_window(conn, win->id);

    cello_add_window_to_desktop(win, win->d);
    cello_update_wilist_with(win->id);

    xcb_flush(conn);
}

static void on_property_notify(xcb_generic_event_t * event) {
    xcb_property_notify_event_t* e = (xcb_property_notify_event_t*)event;

    if (e->atom == ewmh->_NET_WM_STRUT_PARTIAL && ewmh_check_strut(e->window)){
        puts("big pp");
    }
}

static void on_unmap_notify(xcb_generic_event_t * event) {
    xcb_unmap_notify_event_t* e = (xcb_unmap_notify_event_t*)event;

    struct window* w;
    struct window* focused;
    focused = xcb_get_focused_window();
    if ((w = find_window_by_id(e->window))) {
        if (focused && focused->id == w->id) xcb_unfocus();
    }
}

void (*events[0x7f])(xcb_generic_event_t *);

void init_events() {
    events[XCB_KEY_PRESS]           = on_key_press;
    events[XCB_BUTTON_PRESS]        = on_button_press;
    events[XCB_ENTER_NOTIFY]        = on_enter_notify;
    events[XCB_CONFIGURE_NOTIFY]    = on_configure_notify;
    events[XCB_CONFIGURE_REQUEST]   = on_configure_request;
    events[XCB_DESTROY_NOTIFY]      = on_destroy_notify;
    events[XCB_MAP_REQUEST]         = on_map_request;
    events[XCB_PROPERTY_NOTIFY]     = on_property_notify;
    events[XCB_UNMAP_NOTIFY]        = on_unmap_notify;
}
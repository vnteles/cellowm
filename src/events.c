#include "events.h"

#include <stdio.h>

#include "xcb.h"
#include "cello.h"
#include "config.h"
#include "ewmh.h"
#include "cursor.h"
#include "log.h"

// button map | temporary?
static struct button buttons[] = {
    { XCB_BUTTON_INDEX_1,  MOD, MOUSE_MOTION, NO_ROOT, {.i = MOVE_WINDOW} },
    { XCB_BUTTON_INDEX_3,  MOD, MOUSE_MOTION, NO_ROOT, {.i = RESIZE_WINDOW} },

    /*feel free to change this*/
    { XCB_BUTTON_INDEX_3,  MOD, RUN, ROOT_ONLY, {.com = (char *[]){"jgmenu", 0}} },
};

#define each_button \
    (unsigned int i = 0; i < (sizeof(buttons) / sizeof(*buttons)); i++)


//temporary
static void on_key_press(xcb_generic_event_t * event) {
    xcb_key_press_event_t* e = (xcb_key_press_event_t*)event;

    xcb_keysym_t keysym = xcb_get_keysym_from_keycode(e->detail);

    uint16_t i;
    unsigned int keys_len = sizeof(keys) / sizeof(*keys);
    for (i = 0; i < keys_len; i++) {
        if (!keys[i].function || keysym != keys[i].key) continue;
        else if (keys[i].mod_mask != e->state) continue;

        keys[i].function(&keys[i].param);
        break;
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
        if (e->value_mask & MASK) {      \
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

    xcb_configure_window(conn, e->window, mask, values);

    /* DLOG("Window 0x%08x configured with geometry %dx%d at [%d, %d]",
       e->window, e->width, e->height, e->x, e->y) */;
    xcb_flush(conn);
}

static void on_destroy_notify(xcb_generic_event_t * event) {
    xcb_destroy_notify_event_t* e = (xcb_destroy_notify_event_t*)event;
    VARDUMP(e->window);

    struct window* w;
    if ((w = find_window_by_id(e->window))) {
        cello_destroy_window(w);
    }
}

static void on_map_request(xcb_generic_event_t * event) {
    xcb_map_request_event_t* e = (xcb_map_request_event_t*)event;
    struct window* win;
    // skip windows in another desktop
    if (find_window_by_id(e->window) != NULL) return;

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

static void on_client_message(xcb_generic_event_t * event) {
    xcb_client_message_event_t * e = (xcb_client_message_event_t *) event;

    // change the desktop message
    if (e->type == ewmh->_NET_CURRENT_DESKTOP) {
        cello_goto_desktop(e->data.data32[0]);
    }
    if (e->type == ewmh->_NET_ACTIVE_WINDOW) {
        struct window * w = find_window_by_id(e->window);
        if (!w) return;

        xcb_focus_window(w);
    }
}

void (*events[0x7f])(xcb_generic_event_t *);

void init_events() {
    events[XCB_KEY_PRESS]              =      on_key_press;
    events[XCB_BUTTON_PRESS]           =      on_button_press;
    events[XCB_ENTER_NOTIFY]           =      on_enter_notify;
    events[XCB_CONFIGURE_NOTIFY]       =      on_configure_notify;
    events[XCB_CONFIGURE_REQUEST]      =      on_configure_request;
    events[XCB_DESTROY_NOTIFY]         =      on_destroy_notify;
    events[XCB_MAP_REQUEST]            =      on_map_request;
    events[XCB_PROPERTY_NOTIFY]        =      on_property_notify;
    events[XCB_UNMAP_NOTIFY]           =      on_unmap_notify;
    events[XCB_CLIENT_MESSAGE]         =      on_client_message;
}

void MOVE_WINDOW_TO_DESKTOP(const union param* param) {
    struct window* f = xcb_get_focused_window();
    if (!f) return;
    if (param->i == f->d) return;

    cello_unmap_win_from_desktop(f);
    cello_add_window_to_desktop(f, param->i);
    xcb_unfocus();
}


void on_mouse_motion(int8_t action) {
     /*----- grab the pointer location -----*/
    xcb_query_pointer_cookie_t query_cookie;
    query_cookie = xcb_query_pointer(conn, root_screen->root);

    xcb_query_pointer_reply_t * query_reply;
    query_reply = xcb_query_pointer_reply(conn, query_cookie, NULL);

    if (query_reply == NULL)
        return;

    int16_t 
        px = query_reply->root_x, 
        py = query_reply->root_y;

    /*--- get the target window ---*/

    struct window * target;
    target = find_window_by_id(query_reply->child);

    if (!target) 
        return;

    struct geometry wingeo = target->geom;

    /*--- generate the cursor ---*/
    // set the default cursor style
    int cursor_style = CURSOR_POINTER;

    if (action == MOVE_WINDOW) cursor_style = CURSOR_MOVE;

    // if resize action, get the resize orientation
    bool blresize = false;
    if (action == RESIZE_WINDOW) {
        blresize = px < wingeo.x + (wingeo.w / 2);
        cursor_style = blresize ? CURSOR_BOTTOM_LEFT_CORNER : CURSOR_BOTTOM_RIGHT_CORNER;
    }

    xcb_cursor_t cursor;
    cursor = dynamic_cursor_get(cursor_style);


    /*----- grab the pointer -----*/

    /*generate the cookie*/
    xcb_grab_pointer_cookie_t pointer_cookie;
    pointer_cookie = xcb_grab_pointer(
        conn, false, root_screen->root,
        POINTER_MASK, XCB_GRAB_MODE_ASYNC, 
        XCB_GRAB_MODE_ASYNC, XCB_NONE,
        cursor, XCB_CURRENT_TIME
    );

    /*grab the pointer reply*/
    xcb_grab_pointer_reply_t * pointer_reply;
    pointer_reply = xcb_grab_pointer_reply(conn, pointer_cookie, NULL);

    /*unable to grab the pointer*/
    if (pointer_reply == NULL) {
        ufree(query_reply);

        if (check_xcursor_support() == false) 
            xcb_free_cursor(conn, cursor);

        return;
    }

    ufree(pointer_reply);

    /*----- grab the keyboard -----*/

    xcb_grab_keyboard_cookie_t keyboard_cookie;
    keyboard_cookie = xcb_grab_keyboard(
        conn, false, root_screen->root, XCB_CURRENT_TIME,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
    );
    
    xcb_grab_keyboard_reply_t * keyboard_reply;
    keyboard_reply = xcb_grab_keyboard_reply(conn, keyboard_cookie, NULL);
    if (keyboard_reply == NULL) {
        
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        ufree(query_reply);

        if (check_xcursor_support() == false) 
            xcb_free_cursor(conn, cursor);

        return;
    }

    ufree(keyboard_reply);

    xcb_raise_window(target->id);

    /*--- main loop ---*/
    bool motion = true;
    for (;motion;) {
        xcb_generic_event_t * event;
        while (!(event = xcb_poll_for_event(conn))) {
            xcb_flush(conn);
        }


        switch (event->response_type & 0x7F) {
            case XCB_DESTROY_NOTIFY: case XCB_MAP_REQUEST: case XCB_PROPERTY_NOTIFY: case XCB_CONFIGURE_REQUEST:
                handle_event(event);
                break;
            case XCB_FOCUS_OUT: case XCB_KEY_PRESS: case XCB_KEY_RELEASE: case XCB_BUTTON_RELEASE: case XCB_BUTTON_PRESS:
                motion = false;
                break;
            case XCB_MOTION_NOTIFY: {
                /* prepare to do the action */
                xcb_motion_notify_event_t * e = (xcb_motion_notify_event_t *) event;

                switch (action) {
                    case MOVE_WINDOW:
                        xcb_move_window(
                            target,
                            wingeo.x + e->event_x - px,
                            wingeo.y + e->event_y - py
                        );

                        break;
                
                    case RESIZE_WINDOW: {
                            int16_t nw = 0, nh = 0;

                            #define DIMENSION(win_o, event_o, axis) ((int16_t)(win_o + event_o - axis))

                            nh = DIMENSION(wingeo.h, e->event_y, py);
                            if (!blresize) {
                                /*normal right resize*/
                            nw = DIMENSION(wingeo.w, e->event_x, px);

                            } else {
                                /*custom left resize*/

                                nw = DIMENSION(wingeo.w, - e->event_x, - px);


                                // VARDUMP((int)nx);
                                VARDUMP((int)nw);

                                int16_t nx = target->geom.x + target->geom.w - nw;
                                if (nw > WINDOW_MIN_WIDTH)
                                    xcb_move_window(target, nx, target->geom.y);
                            }

                            #undef DIMENSION

                            if (nw < 0) nw = target->geom.w;
                            if (nh < 0) nh = target->geom.h;
                        
                            xcb_resize_window(target, (uint16_t)nw, (uint16_t)nh);
                    }
                    xcb_flush(conn);
                    break;
                }
                break;
            }
            default:
                break;
        }

        ufree(event);
        xcb_flush(conn);
    }
    
    /*----- clean up -----*/
    if (check_xcursor_support() == false) 
        xcb_free_cursor(conn, cursor);

    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
}
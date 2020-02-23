#include "events.h"

#include <stdio.h>

#include "xcb.h"
#include "cello.h"
#include "config.h"
#include "ewmh.h"
#include "cursor.h"
#include "log.h"

#define EV_FUNCTION(FNAME) \
    static void ev_##FNAME(xcb_generic_event_t * event)


EV_FUNCTION(button_press) {

    #define each_button \
        (unsigned int i = 0; i < (sizeof(buttons) / sizeof(*buttons)); i++)

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
        buttons[i].function(buttons[i].action);
    }
}

EV_FUNCTION(enter_notify) {
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

EV_FUNCTION(configure_notify) {
    NLOG("Configure Notify event received");
    xcb_configure_notify_event_t * e = (xcb_configure_notify_event_t *) event;

    if (e->window == root_screen->root) {
        puts("changed something");

        #define applyifchanged(a, b) a = a != b ? b : a

        applyifchanged(root_screen->width_in_pixels, e->width);
        applyifchanged(root_screen->height_in_pixels, e->height);

        #undef applyifchanged
    }
}

EV_FUNCTION(configure_request) {
    puts("config request");
    xcb_configure_request_event_t* e = (xcb_configure_request_event_t*)event;

    uint32_t values[7];
    uint16_t mask = 0;
    uint8_t i = 0;


    struct window * w = find_window_by_id(e->window);

    #define CLONE_MASK(MASK, CONFIG, WMOD)      \
        if (e->value_mask & MASK) {             \
            mask |= MASK;                       \
            values[i++] = e->CONFIG;            \
            if (w && WMOD) *(WMOD) = e->CONFIG; \
        }

    char * nullptr = 0;

    CLONE_MASK(XCB_CONFIG_WINDOW_X, x, &w->geom.x);
    CLONE_MASK(XCB_CONFIG_WINDOW_Y, y, &w->geom.y);
    CLONE_MASK(XCB_CONFIG_WINDOW_WIDTH, width, &w->geom.w);
    CLONE_MASK(XCB_CONFIG_WINDOW_HEIGHT, height, &w->geom.h);
    CLONE_MASK(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width, nullptr);
    CLONE_MASK(XCB_CONFIG_WINDOW_SIBLING, sibling, nullptr);
    CLONE_MASK(XCB_CONFIG_WINDOW_STACK_MODE, stack_mode, nullptr);

    #undef CLONE_MASK

    xcb_configure_window(conn, e->window, mask, values);

    /* DLOG("Window 0x%08x configured with geometry %dx%d at [%d, %d]",
       e->window, e->width, e->height, e->x, e->y) */;
    xcb_flush(conn);
}

EV_FUNCTION(destroy_notify) {
    xcb_destroy_notify_event_t* e = (xcb_destroy_notify_event_t*)event;
    // VARDUMP(e->window);

    struct window* w;
    if ((w = find_window_by_id(e->window))) {
        cello_destroy_window(w);
    }
}

EV_FUNCTION(map_request) {
    xcb_map_request_event_t* e = (xcb_map_request_event_t*)event;
    struct window* w;
    // skip windows in another desktop
    if (find_window_by_id(e->window) != NULL) return;

    // configure window
    if (!(w = window_configure_new(e->window))) return;

    w->d = cello_get_current_desktop();
    xcb_map_window(conn, w->id);

    cello_add_window_to_desktop(w, w->d);
    cello_update_wilist_with(w->id);

    xcb_flush(conn);
}

EV_FUNCTION(property_notify) {
    xcb_property_notify_event_t* e = (xcb_property_notify_event_t*)event;
    // VARDUMP(e->atom);
    // VARDUMP(ewmh->_NET_WM_STRUT_PARTIAL);

    if (e->atom == ewmh->_NET_WM_STRUT_PARTIAL){
        puts("strut partial changed");
    }
    if (e->atom == ewmh->_NET_WM_STRUT) {
        puts("strut changed");
    }
    if (e->atom == XCB_ATOM_WM_HINTS) {
        puts("wm hint");
    }
}

EV_FUNCTION(unmap_notify) {
    xcb_unmap_notify_event_t * e = (xcb_unmap_notify_event_t*)event;

    struct window * w;
    struct window * focused;
    focused = xcb_get_focused_window();
    if ((w = find_window_by_id(e->window))) {
        if (focused && focused->id == w->id)
            xcb_unfocus();
    }
}

EV_FUNCTION(client_message) {
    xcb_client_message_event_t * e = (xcb_client_message_event_t *) event;

    // change desktop
    if (e->type == ewmh->_NET_CURRENT_DESKTOP || e->type == ewmh->_NET_WM_DESKTOP) {
        cello_goto_desktop(e->data.data32[0]);
    }
    
    // change active window
    else if (e->type == ewmh->_NET_ACTIVE_WINDOW) {
        struct window * w = find_window_by_id(e->window);
        if (!w) return;

        xcb_focus_window(w);
    }

    else if (e->type == ewmh->_NET_WM_DESKTOP) {
        struct window * w = find_window_by_id(e->window);
        if (!w) return;

        xcb_change_window_ds(w, e->data.data32[0]);
    }

    else if (e->type == ewmh->_NET_MOVERESIZE_WINDOW) {

        struct window * w = find_window_by_id(e->window);
        if (!w) return;

        w->geom.x = e->data.data32[1];
        w->geom.y = e->data.data32[2];
        w->geom.w = e->data.data32[3];
        w->geom.h = e->data.data32[4];
    }

    else if (e->type == ewmh->_NET_WM_STATE) {
        struct window * w = find_window_by_id(e->window);
        if (w == NULL) return;

        // Maximize window
        if (e->data.data32[1] == ewmh->_NET_WM_STATE_FULLSCREEN || e->data.data32[2] == ewmh->_NET_WM_STATE_FULLSCREEN) {
            switch (e->data.data32[0]) {
                case XCB_EWMH_WM_STATE_ADD:
                    window_maximize(w, CELLO_STATE_MAXIMIZE);
                    break;
                case XCB_EWMH_WM_STATE_REMOVE:
                    cello_unmaximize_window(w);
                    break;
                case XCB_EWMH_WM_STATE_TOGGLE:
                    // if maximized
                    if (!(w->state_mask & CELLO_STATE_NORMAL)) {
                        // turn normal
                        cello_unmaximize_window(w);
                    } else {
                        // turn maximized
                        window_maximize(w, CELLO_STATE_MAXIMIZE);
                    }
                default:
                    break;
            }
        }
    }

    puts("other client message received");
}

void (*events[0x7f])(xcb_generic_event_t *);

void init_events() {
    events[XCB_BUTTON_PRESS]           =      ev_button_press;
    events[XCB_ENTER_NOTIFY]           =      ev_enter_notify;
    events[XCB_CONFIGURE_NOTIFY]       =      ev_configure_notify;
    events[XCB_CONFIGURE_REQUEST]      =      ev_configure_request;
    events[XCB_DESTROY_NOTIFY]         =      ev_destroy_notify;
    events[XCB_MAP_REQUEST]            =      ev_map_request;
    events[XCB_PROPERTY_NOTIFY]        =      ev_property_notify;
    events[XCB_UNMAP_NOTIFY]           =      ev_unmap_notify;
    events[XCB_CLIENT_MESSAGE]         =      ev_client_message;
}



static void resize_helper(struct window * w, struct geometry w_geometry, struct coord coords[static restrict 2], uint8_t gravity) {
    #define POINTERC    0
    #define EVENTC      1

    int16_t nw = 0, nh = 0;

    nh = w_geometry.h + coords[EVENTC].y - coords[POINTERC].y;

    if (gravity == XCB_GRAVITY_NORTH_WEST) {
        nw = w_geometry.w - coords[EVENTC].x + coords[POINTERC].x;

        int16_t nx = w->geom.x + w->geom.w - nw;
        if (nw > WINDOW_MIN_WIDTH)
            xcb_move_window(w, nx, w->geom.y);

    } else {
        nw = w_geometry.w + coords[EVENTC].x - coords[POINTERC].x;
    }


    if (nw < 0) nw = w->geom.w;
    if (nh < 0) nh = w->geom.h;

    xcb_resize_window(w, (uint16_t)nw, (uint16_t)nh);

    #undef POINTERC
    #undef EVENTC
}

void on_mouse_motion(const union action act) {
    uint8_t action = act.i;
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

    struct geometry w_geometry = target->geom;

    /*--- generate the cursor ---*/
    // set the default cursor style
    int cursor_style = CURSOR_POINTER;

    if (action == MOVE_WINDOW) cursor_style = CURSOR_MOVE;

    // if resize action, get the resize orientation
    uint8_t gravity = 0;

    if (action == RESIZE_WINDOW) {
        gravity = px < w_geometry.x + (w_geometry.w / 2) ? XCB_GRAVITY_NORTH_WEST : XCB_GRAVITY_NORTH_EAST;
        cursor_style = gravity == XCB_GRAVITY_NORTH_WEST ? CURSOR_BOTTOM_LEFT_CORNER : CURSOR_BOTTOM_RIGHT_CORNER;
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


    xcb_raise_window(target->id);

    /*--- main loop ---*/
    bool motion = true;
    for (;motion;) {
        xcb_generic_event_t * event;
        while (!(event = xcb_poll_for_event(conn))) {
            xcb_flush(conn);
        }


        switch (event->response_type & 0x7F) {
            case XCB_DESTROY_NOTIFY: case XCB_MAP_REQUEST: case XCB_PROPERTY_NOTIFY:
                handle_event(event);
                break;
            case XCB_FOCUS_OUT: case XCB_KEY_PRESS: case XCB_KEY_RELEASE: case XCB_BUTTON_RELEASE: case XCB_BUTTON_PRESS:
                motion = false;
                break;
            case XCB_MOTION_NOTIFY: {
                /* prepare to do the action */
                xcb_motion_notify_event_t * e = (xcb_motion_notify_event_t *) event;
                if (action == RESIZE_WINDOW) {
                    resize_helper(
                        target, w_geometry,
                        (struct coord[]){
                           {.x = px, .y = py},
                           {.x = e->event_x,.y = e->event_y}
                        },
                        gravity
                    );
                } else if (action == MOVE_WINDOW) {
                    xcb_move_window(
                        target,
                        w_geometry.x + e->event_x - px,
                        w_geometry.y + e->event_y - py
                    );
                }
                update_decoration(target);
                xcb_flush(conn);
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
}
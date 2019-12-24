#include "cello.h"
#include "handlers.h"
#include "list.h"
#include "ewmh.h"
#include "xcb.h"
#include "log.h"
#include "cursor.h"

#include "config.h"

#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <unistd.h>

#define each_button (unsigned int i = 0; i < (sizeof(buttons)/sizeof(*buttons)); i++)
static xcb_generic_event_t * event;

static void HANDLE_XCB_KEY_PRESS() {
    xcb_key_press_event_t * e = (xcb_key_press_event_t *) event;

    xcb_keysym_t keysym = xcb_get_keysym_from_keycode(e->detail);

    uint16_t i;
    unsigned int keys_len = sizeof(keys)/sizeof(*keys);
    for (i = 0; i < keys_len; i++) {
        if (keys[i].function && keysym == keys[i].key) {
            if (keys[i].mod_mask == e->state) {
                keys[i].function(&keys[i].param);
                break;
            }
        }
    }
}

#define NO_MOVEABLE (!f || (f && (f->state_mask & CELLO_STATE_MONOCLE || f->state_mask & CELLO_STATE_MAXIMIZE) ))

static void HANDLE_XCB_BUTTON_PRESS() {
    xcb_button_press_event_t * e = (xcb_button_press_event_t *) event;
 
    for each_button
        if (buttons[i].function && buttons[i].button == e->detail)
            if (buttons[i].mod_mask == e->state){
                struct window * f = xcb_get_focused_window();
                if (buttons[i].function == MOUSE_MOTION && NO_MOVEABLE) continue;

                if (buttons[i].win_mask & ROOT_ONLY){
                    if (e->event == e->root && !e->child) 
                        goto action;
                    continue;
                }

                action:
                buttons[i].function(&buttons[i].param);
                return;
            }
}
static void HANDLE_XCB_ENTER_NOTIFY() {
    xcb_enter_notify_event_t * e = (xcb_enter_notify_event_t *) event;

    if ( e->mode != XCB_NOTIFY_MODE_NORMAL && e->mode != XCB_NOTIFY_MODE_UNGRAB ) return;

    struct window * focused;
    struct window * w;

    focused = xcb_get_focused_window();
    if (focused && e->event == focused->id) return;

    /*don't focus on non mapped windows*/
    if (!(w = cello_find_window(e->event))) return;

    xcb_focus_window(w);
    xcb_flush(conn);
}
static void HANDLE_XCB_CONFIGURE_NOTIFY() {
    NLOG("{@} Configure Notify event received\n");
    // only when set up randr
}

static void HANDLE_XCB_CONFIGURE_REQUEST() {
    xcb_configure_request_event_t * e = (xcb_configure_request_event_t *) event;

    uint32_t values[7];
    uint16_t mask = 0;
    uint8_t i = 0;

    #define CLONE_MASK(MASK, CONFIG) \
        if ( e->value_mask & MASK ){ \
            mask |= MASK; \
            values[i++] = e->CONFIG; \
        }

    CLONE_MASK(XCB_CONFIG_WINDOW_X, x);
    CLONE_MASK(XCB_CONFIG_WINDOW_Y, y);
    CLONE_MASK(XCB_CONFIG_WINDOW_WIDTH, width);
    CLONE_MASK(XCB_CONFIG_WINDOW_HEIGHT, height);
    CLONE_MASK(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width);
    CLONE_MASK(XCB_CONFIG_WINDOW_SIBLING, sibling);
    CLONE_MASK(XCB_CONFIG_WINDOW_STACK_MODE, stack_mode);

    #undef CLONE_MASK


    NLOG("{@} Window 0x%08x configured with geometry %dx%d at [%d, %d]\n", e->window, e->width, e->height, e->x, e->y);

    xcb_configure_window(conn, e->window, mask, values);
    xcb_flush(conn);
}
static void HANDLE_XCB_DESTROY_NOTIFY() {
    xcb_destroy_notify_event_t * e = (xcb_destroy_notify_event_t *) event;

    struct window * w;
    if ((w = cello_find_window(e->window))){
        cello_destroy_window(w);
    }
}

static void HANDLE_XCB_MAP_REQUEST() {
    xcb_map_request_event_t * e = (xcb_map_request_event_t *) event;

    struct window * win;
    // skip if windows in another desktop
    if (cello_find_window(e->window)) return;

    // configure window
    if (!( win = cello_configure_new_window(e->window) )) return;

    win->d = cello_get_current_desktop();
    xcb_map_window(conn, win->id);

    cello_add_window_to_desktop(win, win->d);
    cello_update_wilist_with(win->id);

    xcb_flush(conn);
}

static void HANDLE_XCB_UNMAP_NOTIFY() {
    xcb_unmap_notify_event_t * e = (xcb_unmap_notify_event_t *) event;

    struct window * w;
    struct window * focused;
    focused = xcb_get_focused_window();
    if ((w = cello_find_window(e->window))){
        if (focused && focused->id == w->id) xcb_unfocus();
    }
}

void (*events[])() = { 
    #define xmacro(EV) \
        [EV] = HANDLE_##EV,
        #include "events.xmacro"
    #undef xmacro
};

void handle(xcb_generic_event_t * e) {
    const uint8_t event_len = sizeof(events) / sizeof(*events);
    if (e->response_type < event_len && events[e->response_type]) 
        events[e->response_type]();
}


void RUN (const union param * param) {
    if (fork())
        return;

    setsid();
    execvp((char *)param->com[0], (char **)param->com);
}

void CLOSE_WINDOW(const union param * param) {
    struct window * focused;
    focused = xcb_get_focused_window();


    if (!focused || focused->id == root_screen->root) return;

    if (param->i & KILL){
        goto kill;
    }

    xcb_get_property_cookie_t cprop;
    xcb_icccm_get_wm_protocols_reply_t rprop;
    //try to use iccm delete

    // get protocols
    cprop = xcb_icccm_get_wm_protocols_unchecked(conn, focused->id, ewmh->WM_PROTOCOLS);

    bool nokill = true;
    if (xcb_icccm_get_wm_protocols_reply(conn, cprop, &rprop, NULL) == 1) {
        for (uint32_t i = 0; i < rprop.atoms_len; i++) {

            if (rprop.atoms[i] == WM_DELETE_WINDOW) {

                xcb_send_event(
                    conn, false, focused->id, 
                    XCB_EVENT_MASK_NO_EVENT,
                    (char *) &(xcb_client_message_event_t){
                        .response_type = XCB_CLIENT_MESSAGE,
                        .format = 32,
                        .sequence = 0,
                        .window = focused->id,
                        .type = ewmh->WM_PROTOCOLS,
                        .data.data32 = {
                            WM_DELETE_WINDOW,
                            XCB_CURRENT_TIME
                        }
                    }
                );                
                nokill = true;
                break;
            }
        }

        xcb_icccm_get_wm_protocols_reply_wipe(&rprop);
    }

    if (!nokill) {
        // could not use wm delete
        kill:
        xcb_kill_client(conn, focused->id);
    }
}

void CENTER_WINDOW(const union param * param) {
    struct window * w = xcb_get_focused_window();
    if (param->i & X) {
        cello_center_window_x(w);
        return;
    }
    if (param->i & Y) {
        cello_center_window_y(w);
        return;
    }
    cello_center_window(w);
}

void RELOAD_CONFIG(const union param * param __attribute((unused))) {
    cello_reload();
}

void TOGGLE_BORDER(const union param * param __attribute((unused))) {
    struct window * focused;
    focused = xcb_get_focused_window();
    if (!focused) return;

    if (focused->deco_mask & DECO_BORDER) {
        __SWITCH_MASK__(focused->deco_mask, DECO_BORDER, DECO_NO_BORDER);
    } else {
        __SWITCH_MASK__(focused->deco_mask, DECO_NO_BORDER, DECO_BORDER);
    }

    cello_decorate_window(focused);
}

void TOGGLE_MAXIMIZE(const union param * param __attribute((unused))) {
    struct window * focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_MAXIMIZE) cello_unmaximize_window(focused);
    else cello_maximize_window(focused);
}

void TOGGLE_MONOCLE(const union param * param __attribute__((unused))) {
    struct window * focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_MONOCLE) cello_unmaximize_window(focused);
    else cello_monocle_window(focused);
}

void CHANGE_DESKTOP(const union param * param) {
    cello_goto_desktop(param->i);
}

void MOVE_WINDOW_TO_DESKTOP(const union param * param) {
    struct window * f = xcb_get_focused_window();
    if (!f) return;
    if (param->i == f->d) return;
    
    cello_unmap_win_from_desktop(f);
    cello_add_window_to_desktop(f, param->i);
    xcb_unfocus();
}

/*free fallback cursor*/
#define freefc(cursor) \
    if (!use_xcursor) xcb_free_cursor(conn, cursor);
void MOUSE_MOTION(const union param * param){
    xcb_cursor_t cursor;
    /*generic event*/ 
    xcb_generic_event_t * gevent = NULL;

    /*pointer info*/
    xcb_query_pointer_reply_t * rpointer;
    xcb_query_pointer_cookie_t cpointer;

    /*grab pointer info*/
    xcb_grab_pointer_reply_t * rgrab;
    xcb_grab_pointer_cookie_t cgrab;
    xcb_generic_error_t * error = NULL;

    bool use_xcursor;

    /*grab keyboard info*/
    xcb_grab_keyboard_reply_t * rkeyboard;
    xcb_grab_keyboard_cookie_t ckeyboard;

    /*pointer coords*/
    int16_t px, py;
    /*window coords*/
    int16_t wx, wy;
    /*window dimentions (width & height)*/
    uint16_t ww, wh;

    struct window * focused;
    bool normal_resize = true;

    cpointer = xcb_query_pointer(conn, root_screen->root);

    if ((rpointer = xcb_query_pointer_reply(conn, cpointer, NULL)) == NULL)
        return;

    px = rpointer->root_x;
    py = rpointer->root_y;

    if ((focused = xcb_get_focused_window()) == NULL) {
        free(rpointer);
        return;
    }

    wx = focused->geom.x;
    wy = focused->geom.y;
    ww = focused->geom.w;
    wh = focused->geom.h;


    use_xcursor = check_xcursor_support();

    /*change the cursor base on action*/
    if (param->i == MOVE_WINDOW){
        /*setting "fleur" cursor*/
        cursor = dynamic_cursor_get(CURSOR_MOVE);
    } else if (param->i == RESIZE_WINDOW) {
        /*setting "cursor_bottom_right_corner" cursor*/
        if ((normal_resize = px > wx+(ww/2)))
            cursor = dynamic_cursor_get(CURSOR_BOTTOM_RIGHT_CORNER);
        else
            cursor = dynamic_cursor_get(CURSOR_BOTTOM_LEFT_CORNER);

    } else {
        /*if you want to implement another possible action, you ca change the cursor here*/
        cursor = dynamic_cursor_get(CURSOR_POINTER);
    }

    cgrab = xcb_grab_pointer(
        conn, false, root_screen->root,
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_POINTER_MOTION, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME
    );

    if ((rgrab = xcb_grab_pointer_reply(conn, cgrab, &error)) == NULL) {
        ELOG("{!!} Could not grab pointer [error code: %d]\n", error->error_code);

        free(error);
        freefc(cursor);

        return;
    }
    free(rgrab);

    ckeyboard = xcb_grab_keyboard(
        conn, false, root_screen->root,
        XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
    );

    if ((rkeyboard = xcb_grab_keyboard_reply(conn, ckeyboard, &error)) == NULL) {
        ELOG("{!!} Could not grab keyboard [error_code = %d]\n", error->error_code);

        free(error);
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        freefc(cursor);

        return;
    }

    free(rkeyboard);
    xcb_raise_focused_window();
    //event loop goes here
    for (;focused;) {
        xcb_flush(conn);
        if (gevent) free(gevent);

        while ((gevent = xcb_poll_for_event(conn)) == NULL)
            xcb_flush(conn);

        switch (gevent->response_type) {

        case XCB_BUTTON_RELEASE:
        case XCB_BUTTON_PRESS:
        case XCB_KEY_RELEASE:
        case XCB_KEY_PRESS:
        case XCB_FOCUS_OUT:
            goto end_motion;

        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t * mevent = (xcb_motion_notify_event_t *) gevent;
            if (param->i == MOVE_WINDOW)
                xcb_move_focused_window(wx + mevent->event_x - px, wy + mevent->event_y - py);
            else if (param->i == RESIZE_WINDOW) {
                int16_t nw = 0, nh = 0;

                #define DIMENSION(win_o, event_o, axis) ((int16_t)(win_o+event_o-axis))

                nh = DIMENSION(wh,mevent->root_y,py);
                if (normal_resize){
                    /*normal rigth resize*/

                    nw = DIMENSION(ww,mevent->root_x,px);


                } else {
                    /*cutom left resize*/
                    int16_t nx  = 0;

                    nw = DIMENSION(ww,-mevent->root_x,-px);
                    nx = focused->geom.x + (-nw+focused->geom.w);

                    xcb_move_focused_window(nx, focused->geom.y);
                }

                #undef DIMENSION
                if (nw < 0) nw = focused->geom.w;
                if (nh < 0) nh = focused->geom.h;

                xcb_resize_focused_window((uint16_t)nw, (uint16_t)nh);
            }
            else 
                /*some other non default action*/
                break;

            xcb_flush(conn);
            break;
        }

        case XCB_CONFIGURE_REQUEST:
        case XCB_MAP_REQUEST:
        case XCB_DESTROY_NOTIFY:
        case XCB_UNMAP_NOTIFY:
            events[gevent->response_type]();
            break;
        }
    }
    end_motion:
    if (gevent) free(gevent);
    if (rpointer) free(rpointer);

    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    freefc(cursor);

    xcb_flush(conn);
}
#undef freefc

void handle_events() {
    NLOG("{@} Waiting for events\n");

    for ( ;; ) {
        xcb_flush(conn);
        if (xcb_connection_has_error(conn)) {
            exit(EXIT_FAILURE);
        }

        if ((event = xcb_wait_for_event(conn))){
            handle(event);
            free(event);
        }
    }
}

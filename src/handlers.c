
#define NO_KEYS
#define NO_BUTTONS

#include "handlers.h"

#include <unistd.h>
#include <string.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/randr.h>

#include <string.h>

#include "cello.h"
#include "config.h"
#include "cursor.h"
#include "ewmh.h"
#include "list.h"
#include "log.h"
#include "xcb.h"
#include "message.h"

#include "events.h"

/**
 ** @brief handle the given event with the event list
 ** @param e the event to be handled
 **/
void handle_event(xcb_generic_event_t* e) {
    const uint8_t event_len = 0x7F;
    uint8_t type = e->response_type & 0x7F;
    if ((type) < event_len && events[type]) {
        events[type](e);

        xcb_flush(conn);
        return;
    }
}

void handle_message(char * msg, int msg_len, int fd) {
    //TODO: send error message back to the fd

    int argc = 0, j = 10;
    char ** argv;

    argv = umalloc(sizeof(char *) * j);

    for (int i = 0; i < msg_len; i++) {
        int len = strlen(msg+i);
        argv[argc] = umalloc(len);

        strcpy(argv[argc++], msg+i);
        // puts(argv[argc-1]);
        i+= len;

        if (argc == j) {
            j *= 2;
            urealloc(argv, sizeof(char *) * j);
        }
    }        // puts(argv[argc-1]);

    argv[argc] = NULL;
    parse_opts(argc, argv);
}

void RUN(const union param* param) {
    if (fork()) return;

    setsid();
    execvp(param->com[0], param->com);
}

void CLOSE_WINDOW(const union param* param) {
    struct window* focused;
    focused = xcb_get_focused_window();

    if (!focused || focused->id == root_screen->root) return;

    if (param->i & KILL) {
        goto kill;
    }

    xcb_get_property_cookie_t cprop;
    xcb_icccm_get_wm_protocols_reply_t rprop;

    // try to use icccm delete
    // get protocols
    cprop = xcb_icccm_get_wm_protocols_unchecked(conn, focused->id, ewmh->WM_PROTOCOLS);

    bool nokill = true;
    if (xcb_icccm_get_wm_protocols_reply(conn, cprop, &rprop, NULL) == 1) {
        for (uint32_t i = 0; i < rprop.atoms_len; i++) {
            if (rprop.atoms[i] == WM_DELETE_WINDOW) {
                xcb_send_event(
                    conn, false, focused->id, XCB_EVENT_MASK_NO_EVENT,
                    (char *) &(xcb_client_message_event_t){
                        .response_type = XCB_CLIENT_MESSAGE,
                        .format = 32,
                        .sequence = 0,
                        .window = focused->id,
                        .type = ewmh->WM_PROTOCOLS,
                        .data.data32 = {WM_DELETE_WINDOW, XCB_CURRENT_TIME}});
                nokill = true;
                break;
            }
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&rprop);
    }

    if (!nokill) {
    /*could not use wm delete or kill param passed*/
kill:
        xcb_kill_client(conn, focused->id);
    }
}

void CENTER_WINDOW(const union param* param) {
    struct window* w = xcb_get_focused_window();
    if (param->i & X) {
        center_window_x(w);
        return;
    }
    if (param->i & Y) {
        center_window_y(w);
        return;
    }
    center_window(w);
}

void RELOAD_CONFIG(const union param* param __attribute((unused))) {
    cello_reload();
}

void TOGGLE_BORDER(const union param* param __attribute((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;

    if (focused->state_mask & CELLO_STATE_BORDER)
        __RemoveMask__(focused->state_mask, CELLO_STATE_BORDER);
    else
        __AddMask__(focused->state_mask, CELLO_STATE_BORDER);

    update_decoration(focused);
}

void TOGGLE_MAXIMIZE(const union param* param __attribute((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_MAXIMIZE)
        cello_unmaximize_window(focused);
    else
        window_maximize(focused, CELLO_STATE_MAXIMIZE);
}

void TOGGLE_MONOCLE(const union param* param __attribute__((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_FOCUS)
        cello_unmaximize_window(focused);
    else
        window_maximize(focused, CELLO_STATE_FOCUS);
}

void CHANGE_DESKTOP(const union param* param) { cello_goto_desktop(param->i); }

void MOVE_WINDOW_TO_DESKTOP(const union param* param) {
    struct window* f = xcb_get_focused_window();
    if (!f) return;
    if (param->i == f->d) return;

    cello_unmap_win_from_desktop(f);
    cello_add_window_to_desktop(f, param->i);
    xcb_unfocus();
}


void MOUSE_MOTION(const union param * param) {
    uint32_t action = param->i;

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

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
#include "helpers.h"
#include "list.h"
#include "log.h"
#include "xcb.h"

#include "events.h"

void handle_event(xcb_generic_event_t* e) {
    const uint8_t event_len = sizeof(events) / sizeof(*events);
    if ((e->response_type) < event_len && events[e->response_type & 0x7F]) {
        events[e->response_type & 0x7F](e);
        xcb_flush(conn);
    }
}

#define comp_str(opt, str) ( strcmp(opt, str) == 0 )

/**
 ** @brief find the given argument in the argument list.
 ** @param arg the argument that you're looking for.
 ** @param argc the length of the argument list.
 ** @param argv the argument list.
 ** @return the position of the argument in the argument list, or -1 if unsuccessfully
 **/
int find_arg(char * arg, int argc, char ** argv) {
    for (int i = 0; i < argc; i++)
        if comp_str(argv[i], arg) return i;
    return -1;
}

/**
 ** @brief get the value of the given argument.
 ** @param arg the argument you're looking for.
 ** @param argc the length of the argument list.
 ** @param argv the argument list.
 ** @return a pointer to the value on the argument list, or null if unsuccessfully
 **/
char * get_arg_val(char * arg, int argc, char ** argv) {
    int i = find_arg(arg, argc, argv);
    return i > -1 && i < argc-1 ?  argv[i+1] : NULL;
}

/**
 ** @brief parse the wm option
 ** @param opts a list with the options
 ** @param opts_len the length of the list
 **/
static void parse_wm(char ** opts, int opts_len) {
#define find_opt(opt) (find_arg(opt, opts_len, opts) > -1)
    if find_opt("exit") {
        running = false;
        return;
    }
    if find_opt("reload") {
        DLOG("Reloading");
        return;
    }
    if find_opt("hijack") {
        DLOG("Hijacking")
        return;
    }
#undef find_opt
}

static void parse_opts(int argc, char ** argv) {
    if (comp_str(argv[0], "wm")) {
        parse_wm(++argv, argc);
    }
    else if (comp_str(argv[0], "window")) {
        parse_wm(++argv, argc);
    }

    else {
        DLOG("Unknown option: %s", argv[0]);
    }

}

void handle_message(char * msg, int msg_len, int fd) {
    //TODO: send error message back to the fd

    int argc = 0, j = 10;
    char ** argv;

    argv = umalloc(sizeof(char *) * j);

    for (int i = 0; i < msg_len; i++) {
        if (msg[i] == 0 && msg) {

            if (j == argc) {
                j *= 2;
                urealloc(argv, sizeof(char *) * j);
            }

            argv[argc] = umalloc(strlen(msg));
            strcpy(argv[argc++], msg);
            msg+=i+1;
        }
    }

    // parse the options
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
                    (char*)&(xcb_client_message_event_t){
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
    if (focused->state_mask & CELLO_STATE_MONOCLE)
        cello_unmaximize_window(focused);
    else
        window_maximize(focused, CELLO_STATE_MONOCLE);
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
    int action = param->i;

    /*genererate the cursor*/
    xcb_cursor_t cursor;
    cursor = dynamic_cursor_get(action == MOVE_WINDOW ? CURSOR_MOVE : CURSOR_BOTTOM_RIGHT_CORNER);

     /*----- grab the pointer location -----*/
    xcb_query_pointer_cookie_t query_cookie;
    query_cookie = xcb_query_pointer(conn, root_screen->root);

    xcb_query_pointer_reply_t * query_reply;
    query_reply = xcb_query_pointer_reply(conn, query_cookie, NULL);

    if (query_reply == NULL)
        return;

    /* get the target window */
    struct window * target;
    target = find_window_by_id(query_reply->child);

    if (!target) 
        return;

    /*----- grab the pointer -----*/

    /*generate the cookie*/
    xcb_grab_pointer_cookie_t pointer_cookie;
    pointer_cookie = xcb_grab_pointer(
        conn, false, root_screen->root,
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
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

    /*----- main loop -----*/

    struct geometry wingeo = target->geom;

    int16_t 
        px = query_reply->root_x, 
        py = query_reply->root_y;

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
            
            case RESIZE_WINDOW:
                xcb_resize_window(
                    target,
                    wingeo.w + e->event_x - px,
                    wingeo.h + e->event_y - py
                );
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
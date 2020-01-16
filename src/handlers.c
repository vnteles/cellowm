
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

#include "events.h"

void handle_event(xcb_generic_event_t* e) {
    const uint8_t event_len = sizeof(events) / sizeof(*events);
    if ((e->response_type) < event_len && events[e->response_type]) {
        events[e->response_type](e);
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
    //TODO: send message back to the fd

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

/*free fallback cursor*/
#define freefc(cursor) \
    if (!use_xcursor) xcb_free_cursor(conn, cursor);

void MOUSE_MOTION(const union param* param) {
    xcb_cursor_t cursor = 0;
    /*generic event*/
    xcb_generic_event_t* gevent = NULL;

    /*pointer info*/
    xcb_query_pointer_reply_t* rpointer;
    xcb_query_pointer_cookie_t cpointer;

    /*grab pointer info*/
    xcb_grab_pointer_reply_t* rgrab;
    xcb_grab_pointer_cookie_t cgrab;
    xcb_generic_error_t* error = NULL;

    bool use_xcursor;

    /*grab keyboard info*/
    xcb_grab_keyboard_reply_t* rkeyboard;
    xcb_grab_keyboard_cookie_t ckeyboard;

    /*pointer coords*/
    int16_t px, py;
    /*window coords*/
    int16_t wx, wy;
    /*window dimentions (width & height)*/
    uint16_t ww, wh;

    struct window* target;
    bool normal_resize = true;

    cpointer = xcb_query_pointer(conn, root_screen->root);

    if (!(rpointer = xcb_query_pointer_reply(conn, cpointer, NULL))) return;

    if ( !(target = find_window_by_id(rpointer->child)) || target->id == root_screen->root) {
        ufree(rpointer);
        return;
    }

    px = rpointer->root_x;
    py = rpointer->root_y;

    wx = target->geom.x;
    wy = target->geom.y;
    ww = target->geom.w;
    wh = target->geom.h;

    use_xcursor = check_xcursor_support();

    /*change the cursor based on action*/
    if (param->i == MOVE_WINDOW) {
        /*setting "fleur" cursor*/
        cursor = dynamic_cursor_get(CURSOR_MOVE);
    } else if (param->i == RESIZE_WINDOW) {
        if ((normal_resize = px > wx + (ww / 2)))
            /*setting "bottom_right_corner" cursor*/
            cursor = dynamic_cursor_get(CURSOR_BOTTOM_RIGHT_CORNER);
        else
            /*setting "bottom_left_corner" cursor*/
            cursor = dynamic_cursor_get(CURSOR_BOTTOM_LEFT_CORNER);

    }

    cgrab = xcb_grab_pointer(
        conn, false, root_screen->root,
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
        cursor, XCB_CURRENT_TIME
    );

    if (!(rgrab = xcb_grab_pointer_reply(conn, cgrab, &error))) {
        ELOG("Could not grab pointer [error code: %d]\n", error->error_code);

        if (error) ufree(error);

        freefc(cursor);

        return;
    }
    ufree(rgrab);

    ckeyboard =
            xcb_grab_keyboard(conn, false, root_screen->root, XCB_CURRENT_TIME,
                                                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    if (!(rkeyboard = xcb_grab_keyboard_reply(conn, ckeyboard, &error))) {
        if (error) {
            ELOG("Could not grab keyboard [error_code = %d]\n",
           error->error_code);
            ufree(error);
        }

        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        freefc(cursor);

        return;
    }

    ufree(rkeyboard);
    xcb_raise_focused_window();
    // event loop goes here
    for (; target;) {
        xcb_flush(conn);
        if (gevent) ufree(gevent);

        while (!(gevent = xcb_poll_for_event(conn))) xcb_flush(conn);

        switch (gevent->response_type) {
            case XCB_BUTTON_RELEASE:
            case XCB_BUTTON_PRESS:
            case XCB_KEY_RELEASE:
            case XCB_KEY_PRESS:
            case XCB_FOCUS_OUT:
                goto end_motion;

            case XCB_MOTION_NOTIFY: {
                xcb_motion_notify_event_t* mevent = (xcb_motion_notify_event_t*)gevent;
                if (param->i == MOVE_WINDOW) {
                    xcb_move_window(
                        target,
                        wx + mevent->event_x - px,
                        wy + mevent->event_y - py
                    );
                }

                else if (param->i == RESIZE_WINDOW) {
                    int16_t nw = 0, nh = 0;

#define DIMENSION(win_o, event_o, axis) ((int16_t)(win_o + event_o - axis))

                    nh = DIMENSION(wh, mevent->root_y, py);
                    if (normal_resize) {
                        /*normal right resize*/
                        nw = DIMENSION(ww, mevent->root_x, px);

                    } else {
                        /*custom left resize*/
                        int16_t nx = 0;

                        nw = DIMENSION(ww, -mevent->root_x, -px);
                        nx = target->geom.x + (-nw + target->geom.w);

                        xcb_move_focused_window(nx, target->geom.y);
                    }

#undef DIMENSION

                    if (nw < 0) nw = target->geom.w;
                    if (nh < 0) nh = target->geom.h;

                    xcb_resize_focused_window((uint16_t)nw, (uint16_t)nh);
                } else
                    /*some other non default action*/
                    break;

                xcb_flush(conn);
                break;
            }

            case XCB_PROPERTY_NOTIFY:
            case XCB_CONFIGURE_REQUEST:
            case XCB_MAP_REQUEST:
            case XCB_DESTROY_NOTIFY:
                events[gevent->response_type](gevent);
                break;
        }
    }
end_motion:
    if (gevent) ufree(gevent);
    if (rpointer) ufree(rpointer);

    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    freefc(cursor);

    xcb_flush(conn);
}
#undef freefc
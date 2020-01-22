
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
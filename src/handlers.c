
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

/**
 ** @brief handle the given message
 ** @param msg the message to be handled
 ** @param msg_len the length of tre message
 ** @param fd the file descriptor of the client
 **/
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
    }

    argv[argc] = NULL;
    parse_opts(argc, argv);
}

void RUN(const union action param) {
    if (fork()) return;

    setsid();
    execvp(param.com[0], param.com);
}

void CLOSE_WINDOW(const union action param) {
    struct window* focused;
    focused = xcb_get_focused_window();

    if (!focused || focused->id == root_screen->root) return;

    xcb_close_window(focused->id, param.i);
}

void CENTER_WINDOW(const union action param) {
    struct window* w = xcb_get_focused_window();
    if (param.i & X) {
        center_window_x(w);
        return;
    }
    if (param.i & Y) {
        center_window_y(w);
        return;
    }
    center_window(w);
}

void RELOAD_CONFIG(const union action param __attribute((unused))) {
    cello_reload();
}

void TOGGLE_BORDER(const union action param __attribute((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;

    if (focused->state_mask & CELLO_STATE_BORDER)
        __RemoveMask__(focused->state_mask, CELLO_STATE_BORDER);
    else
        __AddMask__(focused->state_mask, CELLO_STATE_BORDER);

    update_decoration(focused);
}

void TOGGLE_MAXIMIZE(const union action param __attribute((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_MAXIMIZE)
        cello_unmaximize_window(focused);
    else
        window_maximize(focused, CELLO_STATE_MAXIMIZE);
}

void TOGGLE_MONOCLE(const union action param __attribute__((unused))) {
    struct window* focused;
    focused = xcb_get_focused_window();
    if (!focused) return;
    /*already maximized*/
    if (focused->state_mask & CELLO_STATE_FOCUS)
        cello_unmaximize_window(focused);
    else
        window_maximize(focused, CELLO_STATE_FOCUS);
}

void CHANGE_DESKTOP(const union action param) { cello_goto_desktop(param.i); }
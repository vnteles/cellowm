#define NO_BUTTONS
#include "cello.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include <pthread.h>

#include "config.h"
#include "confparse.h"
#include "cursor.h"
#include "ewmh.h"
#include "handlers.h"
#include "list.h"
#include "log.h"
#include "xcb.h"
#include "events.h"

xcb_connection_t *conn = NULL;
xcb_screen_t *root_screen = NULL;

bool run;

static int sockfd, connfd;
char wmsockp[256];

int scrno;

struct config conf;

static uint32_t current_ds = 0;

static bool cello_setup_conn() {
    NLOG("Setting up the %s connection\n", WMNAME);

    scrno = -1;

    conn = xcb_connect(NULL, &scrno);

    if (xcb_connection_has_error(conn))
        CRITICAL("Could not open the connection to the X server");

    NLOG("Connection generated..Checking for the socket");
    /*generate the connection socket*/
    struct sockaddr_un addr;

    connfd = xcb_get_file_descriptor(conn);

    char * host = NULL;
    int dsp, scr;

    if (xcb_parse_display(NULL, &host, &dsp, &scr) != 0)
        if (snprintf(addr.sun_path, sizeof addr.sun_path, WMSOCKPATH, host, dsp, scr) < 0)
            CRITICAL("Could not get the socket path");

    strncpy(wmsockp, addr.sun_path, sizeof wmsockp);

    if (host)
        ufree(host);

    // create the socket
    addr.sun_family = AF_UNIX;
    if ((sockfd = socket(addr.sun_family, SOCK_STREAM, 0)) == -1)
        CRITICAL("Could not open the socket file descriptor");

    unlink(wmsockp);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof addr) == -1)
        CRITICAL("Could not create the socket file");

    if (listen(sockfd, SOMAXCONN) == -1)
        CRITICAL("Could not listen to the socket");

    NLOG("Socket created successfully\n");

    return scrno == -1 ? false : true;
}

void cello_init_config() {
    conf.border = false;
    conf.focus_gap = 30;
    conf.inner_border = 1;
    conf.outer_border = 1;

    //solid red
    conf.inner_border_color = 0xff000000 | 0xff0000;
    //solid green
    conf.outer_border_color = 0xff000000 | 0x00ff00;

    conf.desktop_number = DEFAULT_DESKTOP_NUMBER;

    // doing that way so we can free every config change, then restore it's defaults if needed
    for (uint32_t i = 0; i < DEFAULT_DESKTOP_NUMBER; i++) {
        size_t len = strlen(DEFAULT_DESKTOP_NAMES[i]);
        conf.desktop_names[i] = umalloc(len);

        memcpy(conf.desktop_names[i], DEFAULT_DESKTOP_NAMES[i], len);
        conf.desktop_names[i][len] = '\0';
    }

    conf.config_ok = false;
}

/*close the connection cleaning the window list*/
void cello_clean() {
    DLOG("Cleaning up..");
    if (wilist) {
        struct window_list *node = wilist;

        for (node = wilist; node && node->window; node = node->next) {
            xcb_unmap_window(conn, node->window->id);
            free_node(&wilist, node);
        }

        ufree(wilist);
    }
    if (ewmh) {
        xcb_ewmh_connection_wipe(ewmh);
        ufree(ewmh);
    }
    if (conn) {
        xcb_flush(conn);
        xcb_disconnect(conn);
    }

    conn = NULL;
}

void cello_exit() {
    NLOG("Exiting "WMNAME);
    exit(EXIT_SUCCESS);
}

__always_inline static void cello_setup_cursor() {
    init_cursors();
    dynamic_cursor_set(CURSOR_POINTER);
}

// desktop
uint32_t cello_get_current_desktop() {
    uint32_t current = 0;

    xcb_get_property_cookie_t current_cookie;
    current_cookie = xcb_ewmh_get_current_desktop(ewmh, scrno);

    uint8_t current_reply;
    current_reply = xcb_ewmh_get_current_desktop_reply(ewmh, current_cookie, &current, NULL);

    if (!current_reply)
        CRITICAL("{!!} Could not get the desktop list");

    return current;
}

// desktop
void cello_add_window_to_desktop(struct window *w, uint32_t ds) {

    if (!w)
        return;

    struct window_list *node;
    if (!(node = new_empty_node(&dslist[ds])))
        return;

    w->dlist = node;
    w->d = ds;
    node->window = w;

    // change window desktop in ewmh
    xcb_ewmh_set_wm_desktop(ewmh, w->id, w->d);

    /*verify if window can stay mapped*/
    {
        struct window *fw = NULL;

        if (dslist[ds] && dslist[ds]->window)
            fw = (struct window *)dslist[ds]->window;

        if (cello_get_current_desktop() != ds || (fw && !(fw->state_mask & CELLO_STATE_NORMAL))) {
            xcb_unmap_window(conn, w->id);
            return;
        }
    }

    update_decoration(w);
}

// desktop
void cello_add_window_to_current_desktop(struct window *w) {
    cello_add_window_to_desktop(w, cello_get_current_desktop());
}


#define get(w, l) w = (struct window *)l->window
#define get_and_map(w, l)                                                      \
    get(w, l); xcb_map_window(conn, w->id);
#define get_and_unmap(w, l)                                                    \
    get(w, l); xcb_unmap_window(conn, w->id);

// window
void cello_unmaximize_window(struct window *w) {
    if (!w)
        return;

    uint32_t current;
    struct window *win;
    struct window_list *aux;

    /*reconfigure the state*/
    __RemoveMask__(w->state_mask, CELLO_STATE_MAXIMIZE);
    __RemoveMask__(w->state_mask, CELLO_STATE_FOCUS);

    /*revert to the original mask values*/
    __AddMask__(w->state_mask, w->tmp_state_mask);
    w->tmp_state_mask = 0;

    w->geom = w->orig;
    xcb_move_window(w, w->geom.x, w->geom.y);
    xcb_resize_window(w, w->geom.w, w->geom.h);

    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, w->id, ewmh->_NET_WM_STATE,
        XCB_ATOM_ATOM, 32, 0, NULL
    );

    current = cello_get_current_desktop();
    /*and map all windows*/
    for (aux = dslist[current]; aux; aux = aux->next) {
        win = aux->window;
        if (win->id != w->id) {
            xcb_map_window(conn, win->id);
            // xcb_map_window(conn, win->frame);
        }
    }

    update_decoration(w);
    xcb_flush(conn);
}

// desktop
void cello_goto_desktop(uint32_t d) {
    uint32_t current = cello_get_current_desktop();
    if (d == current || d >= conf.desktop_number)
        return;

    struct window * w;
    struct window_list *aux;

    /*unmap all windows from the old desktop*/
    for (aux = dslist[current]; aux; aux = aux->next) {
        get_and_unmap(w, aux);
    }

    /*then map again the windows on the current desktop*/
    if ((aux = dslist[d])) {
        get_and_map(w, aux);

        /*check if there are some maximized window
     *(maximized/focus windows are supposed to be always at the first
     *position)
     */
        if (w->state_mask & CELLO_STATE_MAXIMIZE ||
                w->state_mask & CELLO_STATE_FOCUS)
            goto end_map;
        /*continue mapping the windows*/
        else
            for (aux = aux->next; aux; aux = aux->next) {
                get_and_map(w, aux);
            }
    }
end_map:
    ewmh_change_to_desktop(0, d);
    xcb_unfocus();

    xcb_query_pointer_reply_t *rpointer = xcb_query_pointer_reply(
            conn, xcb_query_pointer(conn, root_screen->root), NULL);

    if (!rpointer)
        return;

    xcb_focus_window(find_window_by_id(rpointer->child));
    ufree(rpointer);
    xcb_flush(conn);
}
#undef get_and_unmap
#undef get_and_map
#undef get

// window
void cello_update_wilist_with(xcb_window_t wid) {
    xcb_change_property(
        conn, XCB_PROP_MODE_APPEND, root_screen->root,
        ewmh->_NET_CLIENT_LIST, 
        XCB_ATOM_WINDOW, 32, 1, &wid
    );
    xcb_change_property(
        conn, XCB_PROP_MODE_APPEND, root_screen->root,
        ewmh->_NET_CLIENT_LIST_STACKING,
        XCB_ATOM_WINDOW, 32, 1, &wid
    );
    xcb_change_property(
        conn, XCB_PROP_MODE_APPEND, root_screen->root,
        XA_WIN_CLIENT_LIST,
        XCB_ATOM_WINDOW, 32, 1, &wid
    );
}

// window
void cello_update_wilist() {
    xcb_query_tree_cookie_t tcookie = xcb_query_tree(conn, root_screen->root);
    xcb_query_tree_reply_t *reply = xcb_query_tree_reply(conn, tcookie, 0);

    if (!reply) {
        cello_update_wilist_with(0);
        return;
    }

    xcb_delete_property(conn, root_screen->root, ewmh->_NET_CLIENT_LIST);
    xcb_delete_property(conn, root_screen->root, ewmh->_NET_CLIENT_LIST_STACKING);
    xcb_delete_property(conn, root_screen->root, XA_WIN_CLIENT_LIST);

    int tlen = xcb_query_tree_children_length(reply);
    xcb_window_t *tch = xcb_query_tree_children(reply);

    struct window *w;
    int i;

    for (i = 0; i < tlen; i++)
        if ((w = find_window_by_id(tch[i])))
            cello_update_wilist_with(w->id);

    ufree(reply);
}

// window
void cello_unmap_win_from_desktop(struct window *w) {
    if (!w)
        return;

    struct window *focused = xcb_get_focused_window();
    if (focused && focused->id == w->id)
        xcb_unfocus();

    struct window_list * pnode = pop_node(&dslist[w->d], w->dlist);
    if (pnode) ufree(pnode);

    w->d = -1;
    w->dlist = NULL;

    xcb_unmap_window(conn, w->id);

    cello_update_wilist();
}

// xcb
void cello_unmap_window(struct window *w) {
    if (!w)
        return;

    cello_unmap_win_from_desktop(w);
    free_node(&wilist, w->wlist);
}

#define each_window (window = wilist; window; window = window->next)
// xcb
void cello_destroy_window(struct window *w) {
    cello_unmap_window(w);
}
#undef each_window


xcb_atom_t new_atom(char * atom_name) {
    xcb_atom_t atom;

    xcb_intern_atom_cookie_t atom_cookie;
    atom_cookie = xcb_intern_atom(conn, false, 16, atom_name);

    xcb_intern_atom_reply_t * atom_reply;
    atom_reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);

    if (!atom_reply) {
        return 0;
    }

    atom = atom_reply->atom;
    free(atom_reply);

    return atom;
}

// atoms
void cello_init_atoms() {
    /*only one atom for now*/

    XA_WM_DELETE_WINDOW = new_atom("WM_DELETE_WINDOW");
    XA_NET_WM_WINDOW_OPACITY = new_atom("_NET_WM_WINDOW_OPACITY");
    XA_WIN_CLIENT_LIST = new_atom("_WIN_CLIENT_LIST");

}

void cello_reload() {
    cello_clean();

    execvp(path, (char * []){path, NULL});
}

void cello_setup_all() {
    NLOG("Initializing "WMNAME"\n");

    // pthread_t config_thread;
    // char *config_file;
    // bool config_open;
    current_ds = 0;

    atexit(cello_exit);

    // config_file = NULL;
    // /*get the file status*/
    // config_open = cello_read_config_file(&config_thread, config_file);

    if (cello_setup_conn() == false)
        CRITICAL("cello_setup_conn: Could not setup the connection");

    if (!(root_screen = xcb_get_root_screen(conn, scrno)))
        CRITICAL("cello_setup_all: Setup failed");


    NLOG("Screen dimensions %dx%d\n", root_screen->width_in_pixels,
       root_screen->height_in_pixels);

    init_events();
    cello_init_config();

    ewmh_init(conn);
    cello_init_atoms();

    ewmh_create_desktops(scrno, conf.desktop_number);
    ewmh_change_desktop_names_list(scrno, conf.desktop_number, conf.desktop_names);

    ewmh_create_ewmh_window(conn, scrno);

    cello_goto_desktop(current_ds);

    NLOG("Initializing %s\n", WMNAME);

    xcb_set_root_def_attr();
    cello_setup_cursor();

    // wait for the configuration before setup the windows
    // if (config_open) {
    //     pthread_join(config_thread, NULL);
    //     ufree(config_file);
    // }

    xcb_grab_server(conn);
    xcb_generic_event_t *event;
    // handle all events before hijack the windows
    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            ufree(event);
            continue;
        }

        int event_type = (event->response_type & 0x7F);

        if (event_type == XCB_MAP_REQUEST)
            handle_event(event);

        if (event)
            ufree(event);
    }

    window_hijack();
    xcb_ungrab_server(conn);

    NLOG("Setup completed!");
}

void cello_deploy() {
    NLOG("Deploying "WMNAME);

    fd_set rfds;

    xcb_generic_event_t * event;

    run = true;
    while (run) {
        xcb_flush(conn);

        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        FD_SET(connfd, &rfds);

        // check which fd is ready
        if (select(max(sockfd, connfd) + 1, &rfds, NULL, NULL, NULL) == -1) continue;

        if (FD_ISSET(connfd, &rfds) != 0) {
            while ((event = xcb_poll_for_event(conn)) != NULL) {
                handle_event(event);
                if (event)
                    ufree(event);
            }
        }
        // handle the ipc messages
        if (FD_ISSET(sockfd, &rfds) != 0) {
            DLOG("Ipc message received");
            int fd, recvlen;
            char recvbuff[BUFSIZ];

            if ((fd = accept(sockfd, NULL, 0)) == -1) {
                DLOG("Unable to accept the socket connection");
                continue;
            }

            if ((recvlen = recv(fd, recvbuff, (sizeof recvbuff) - 1, 0)) == -1) {
                DLOG("Something went wrong receiving the socket message");
                continue;
            }

            if (!recvbuff[0]) {
                DLOG("Ipc message is empty");
                continue;
            }

            recvbuff[recvlen] = '\0';

            handle_message(recvbuff, recvlen, fd);
        }

        if (xcb_connection_has_error(conn))
            run = false;
    }
    // handle_events();
    exit(EXIT_SUCCESS);
}

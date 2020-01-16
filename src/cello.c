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

bool running;

struct list *dslist[MAX_DESKTOPS];

static int sockfd, connfd;
char wmsockp[256];

struct config conf;
FILE *logfp;

static uint32_t current_ds = 0;

static int cello_setup_conn() {
    NLOG("Setting up the %s connection\n", WMNAME);

    int scrno;

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

    return scrno;
}

/*close the connection cleaning the window list*/
void cello_clean() {
    DLOG("Cleaning up..");
    if (wilist) {
        struct list *node;

        for (node = wilist; node; node = node->next) {

            struct window *w = (struct window *)node->gdata;
            xcb_unmap_window(conn, w->id);
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


// xcb

// window


// desktop
uint32_t cello_get_current_desktop() {
    uint32_t current = 0;

    if (
        xcb_ewmh_get_current_desktop_reply(
        ewmh, xcb_ewmh_get_current_desktop(ewmh, 0), &current, NULL)
    )
        return current;

    CRITICAL("{!!} Could not get the desktop list");
}

// desktop
void cello_add_window_to_desktop(struct window *w, uint32_t ds) {

    if (!w)
        return;

    struct list *node;
    if (!(node = new_empty_node(&dslist[ds])))
        return;
    w->dlist = node;
    w->d = ds;
    node->gdata = (unsigned char *)w;

    // change window desktop in ewmh
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->id, ewmh->_NET_WM_DESKTOP,
                                            XCB_ATOM_CARDINAL, 32, 1, &w->d);

    /*verify if window can stay mapped*/
    {
        uint32_t c = cello_get_current_desktop();
        struct window *fw = NULL;

        if (dslist[c] && dslist[c]->gdata)
            fw = (struct window *)dslist[c]->gdata;

        if (w->d != c || (fw && !(fw->state_mask & CELLO_STATE_NORMAL))) {
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

#define each_window_in_ds(ds) (node = dslist[ds]; node; node = node->next)

// window

// window
// deprecated
void cello_monocle_window(struct window *w) {
    if (!w)
        return;
    if (w->state_mask & CELLO_STATE_MONOCLE)
        return;

    struct list *node;
    uint32_t current;

    current = cello_get_current_desktop();
    /*cannot maximize a window out of the current desktop*/
    if (!dslist[current] || w->d != current)
        return;

    /*unmap other windows before maximize*/
        for
            each_window_in_ds(current) {
                struct window *data = (struct window *)node->gdata;
                if (data->id != w->id) {
                    xcb_unmap_window(conn, data->id);
                    // xcb_unmap_window(conn, data->frame);
                }
            }

        /*save the original geometry and state*/
        if (!(w->state_mask & CELLO_STATE_MAXIMIZE)) {
            __AddMask__(w->tmp_state_mask, w->state_mask);
            w->orig = w->geom;
        }

        w->geom.w = root_screen->width_in_pixels;
        w->geom.h = root_screen->height_in_pixels;

        if (root_screen->width_in_pixels > conf.monocle_gap)
            w->geom.w -= conf.monocle_gap;
        if (root_screen->height_in_pixels > conf.monocle_gap)
            w->geom.h -= conf.monocle_gap;

        uint32_t smask = w->state_mask;

        __SwitchMask__(w->state_mask, CELLO_STATE_BORDER | CELLO_STATE_MAXIMIZE,
                   CELLO_STATE_NORMAL);

        center_window(w);
        xcb_resize_window(w, w->geom.w, w->geom.h);
        w->state_mask = smask;

        /*set maximized state*/
        __SwitchMask__(w->state_mask, CELLO_STATE_MAXIMIZE, CELLO_STATE_MONOCLE);

        move_to_head(&dslist[cello_get_current_desktop()], w->dlist);
        xcb_flush(conn);
}

#undef each_window_in_ds

#define get(w, l) w = (struct window *)l->gdata
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
    struct list *aux;

    /*reconfigure the state*/
    __RemoveMask__(w->state_mask, CELLO_STATE_MAXIMIZE | CELLO_STATE_MONOCLE);
    /*revert to the original mask values*/
    __AddMask__(w->state_mask, w->tmp_state_mask);
    w->tmp_state_mask = 0;

    w->geom = w->orig;
    xcb_move_window(w, w->geom.x, w->geom.y);
    xcb_resize_window(w, w->geom.w, w->geom.h);

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->id, ewmh->_NET_WM_STATE,
                                            XCB_ATOM_ATOM, 32, 0, NULL);

    current = cello_get_current_desktop();
    /*and map all windows*/
    for (aux = dslist[current]; aux; aux = aux->next) {
        __Node2Window__(aux, win);
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
    if (d == current || d > MAX_DESKTOPS)
        return;

    struct window *w;
    struct list *aux;

    /*unmap all windows from the old desktop*/
    for (aux = dslist[current]; aux; aux = aux->next) {
        get_and_unmap(w, aux);
    }

    /*then map again the windows on the current desktop*/
    if ((aux = dslist[d])) {
        get_and_map(w, aux);

        /*check if there are some maximized window
     *(maximized/monocle windows are supposed to be always at the first
     *position)
     */
        if (w->state_mask & CELLO_STATE_MAXIMIZE ||
                w->state_mask & CELLO_STATE_MONOCLE)
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
void cello_update_wilist_with(xcb_drawable_t wid) {
    xcb_change_property(conn, XCB_PROP_MODE_APPEND, root_screen->root,
                                            ewmh->_NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, 1, &wid);
    xcb_change_property(conn, XCB_PROP_MODE_APPEND, root_screen->root,
                                            ewmh->_NET_CLIENT_LIST_STACKING, XCB_ATOM_WINDOW, 32, 1,
                                            &wid);
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
    pop_node(&dslist[w->d], w->dlist);

    w->d = -1;
    w->dlist = NULL;

    xcb_unmap_window(conn, w->id);
    // xcb_unmap_window(conn, w->frame);

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
    struct list *window;
        for
            each_window {
                struct window *win = (struct window *)window->gdata;
                if (win->id == w->id) {
                    cello_unmap_window(w);
                    return;
                }
            }
}
#undef each_window

void cello_grab_keys() {

    /*ungrab any key that for some reason was grabbed*/
    xcb_ungrab_key(conn, XCB_GRAB_ANY, root_screen->root, XCB_MOD_MASK_ANY);
    unsigned int i, j;
    const unsigned int keys_len = sizeof(keys) / sizeof(*keys);
    for (i = 0; i < keys_len; i++) {
        xcb_keycode_t *keycode = xcb_get_keycode_from_keysym(keys[i].key);
        if (!keycode)
            return;

        for (j = 0; keycode[j]; j++) {
            xcb_grab_key(conn, true, root_screen->root, keys[i].mod_mask, keycode[j],
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            xcb_grab_key(conn, true, root_screen->root,
                   keys[i].mod_mask | XCB_MOD_MASK_LOCK, keycode[j],
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }

        ufree(keycode);
    }
}

// atoms
void cello_init_atoms() {
    /*only one atom for now*/
    xcb_intern_atom_cookie_t catom =
            xcb_intern_atom(conn, false, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *ratom = xcb_intern_atom_reply(conn, catom, NULL);

    if (!ratom) {
        WM_DELETE_WINDOW = 0;
        return;
    }

    WM_DELETE_WINDOW = ratom->atom;
    ufree(ratom);
}

// remove

// confparse

/* Return true if the thread could start,
 * so we can handle the pthread_join.
 * The config file must be passed so it
 * can be freed posteriorly with pthread_join.
 */
bool cello_read_config_file(pthread_t *config_thread, char *config_file) {
    if (!config_thread)
        return false;
    conf.config_ok = false;

    uint8_t baselen = 25;

    config_file = ucalloc(baselen, sizeof(*config_file));
    strncpy(config_file, getenv("HOME"), baselen);

    /* --- reallocate the config file to fit it's real size */
    baselen = strlen(config_file);
    if (baselen <3) {
        if (config_file)
            ufree(config_file);
        return false;
    }
    config_file = urealloc(config_file, CONFIG_PATH_LEN + baselen);
    /* --- concat the config path */
    strcat(config_file, CONFIG_PATH);

    NLOG("Looking for %s\n", config_file);
    /*check if file exists and can be read*/
    if (access(config_file, F_OK | R_OK) != -1) {
        pthread_create(config_thread, NULL, (void *(*)(void *)) & parse_json_config,
                   config_file);
        return true;
    }
    DLOG("Configuration file not found");
    ufree(config_file);
    return false;
}

void cello_reload() {

/* dev reload can reload the execution, so we can test changes on each compile
 */
#ifndef DEVRELOAD
    /* save the current ds to return to the same desktop when the wm reload */
    current_ds = cello_get_current_desktop();

    cello_clean();

    if (cello_setup_all())
        cello_deploy();
#else

    cello_clean();

    extern char *execpath;
    execvp(execpath, (char *[]){execpath, NULL});
#endif
}

bool cello_setup_all() {
    pthread_t config_thread;
    char *config_file;
    bool config_open;

    int scr;
    current_ds = 0;

    NLOG("Initializing %s\n", WMNAME);
    atexit(cello_exit);

    config_file = NULL;
    /*get the file status*/
    config_open = cello_read_config_file(&config_thread, config_file);

    if ((scr = cello_setup_conn()) < 0)
        return false;

    if (!(root_screen = xcb_get_root_screen(conn, scr))) {
        ELOG("cello_setup_all: Setup failed");
        return false;
    }

    NLOG("Screen dimensions %dx%d\n", root_screen->width_in_pixels,
       root_screen->height_in_pixels);
    
    init_events();

    ewmh_init(conn);
    cello_init_atoms();

    ewmh_create_desktops(scr, MAX_DESKTOPS);
    ewmh_create_ewmh_window(conn, scr);

    cello_goto_desktop(current_ds);

    NLOG("Initializing %s\n", WMNAME);

    cello_grab_keys();

    xcb_set_root_def_attr();
    cello_setup_cursor();

    // wait for the configuration before setup the windows
    if (config_open) {
        pthread_join(config_thread, NULL);
        ufree(config_file);
    }

    // ignore all events while the wm is starting
    xcb_grab_server(conn);
    xcb_generic_event_t *event;
    // handle all events before hijack the windows
    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            // prevent possible errors caused by memory corruption or something related 
            if (event)
                ufree(event);
            continue;
        }

        int type = (event->response_type & 0x7F);

        if (type == XCB_MAP_REQUEST)
            handle_event(event);

        if (event)
            ufree(event);
    }

    window_hijack();
    xcb_ungrab_server(conn);

    NLOG("Setup completed!");
    return true;
}

void cello_deploy() {
    NLOG("Deploying "WMNAME);

    running = true;
    fd_set rfds;

    xcb_generic_event_t * event;

    while (running) {
        xcb_flush(conn);
        
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        FD_SET(connfd, &rfds);

        // get the fd ready
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
            running = false;
    }
    // handle_events();
    exit(EXIT_SUCCESS);
}
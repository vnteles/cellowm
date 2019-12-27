#define NO_BUTTONS
#include "cello.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include <pthread.h>

#include "log.h"
#include "confparse.h"
#include "ewmh.h"
#include "cursor.h"
#include "list.h"
#include "handlers.h"
#include "config.h"
#include "xcb.h"

xcb_connection_t * conn = NULL;
xcb_screen_t * root_screen = NULL;

struct list * wilist;
struct list * dslist[MAX_DESKTOPS];

struct config conf;
FILE * logfp;


static int cello_setup_conn() {
    NLOG("{@} Setting up the %s connection\n", WMNAME);

    int scrno;

    conn = xcb_connect(NULL, &scrno);

    if (!xcb_connection_has_error(conn))
        return scrno;

    ELOG("{!} Could not open the Xdisplay\n");

    return -1;
}

/*close the connection cleaning the window list*/
void cello_clean() {
    if (wilist){
        struct list * node;

        for (node = wilist; node; node=node->next){

            struct window * w = (struct window *) node->gdata;
            xcb_unmap_window(conn, w->id);

            free_node(&wilist, node);
        }

        free(wilist);
    }
    if (ewmh) {
        xcb_ewmh_connection_wipe(ewmh);
        free(ewmh);
    }
    if (conn) {
        xcb_disconnect(conn);
    }

    ewmh = NULL;
    wilist = NULL;
    conn = NULL;
}

void cello_exit() {
    NLOG("\n{@} Exiting %s\n", WMNAME);
    exit(EXIT_SUCCESS);
}

__always_inline
static void cello_setup_cursor() {
    init_cursors();
    dynamic_cursor_set(CURSOR_POINTER);
}

// xcb
static void cello_change_root_default_attributes() {
    (void) xcb_change_window_attributes(
        conn, root_screen->root, XCB_CW_EVENT_MASK, (uint32_t[1]) { ROOT_EVENT_MASK }
    );
}

// window
struct window * cello_find_window(xcb_drawable_t wid){
    struct window * win;
    struct list * list;

    for (list = wilist; list; list=list->next){
        win = (struct window *) list->gdata;
        if (win->id == wid){
            return win;
        }
    }

    return NULL;
}

// window
static void fill_geometry(xcb_drawable_t wid, int16_t * x, int16_t * y, uint16_t * w, uint16_t * h, uint8_t * depth) {
    xcb_get_geometry_cookie_t gcookie = xcb_get_geometry(conn, wid);
    xcb_get_geometry_reply_t  * geo;

    if (!( geo = xcb_get_geometry_reply(conn, gcookie, NULL) )) return;

    *x = geo->x;
    *y = geo->y;
    *w = geo->width;
    *h = geo->height;
    *depth = geo->depth;
}

// window

// window
struct window * cello_configure_new_window(xcb_window_t win) {
    struct window * w;
    struct list * node;

    if (ewmh_is_special_window(win)) {
        xcb_map_window(conn, win);
        return NULL;
    }

    w = umalloc(sizeof(struct window));

    if (!( node = new_empty_node(&wilist) )) return NULL;
    node->gdata = (char *) w;

    w->id = win;

    xcb_change_window_attributes_checked(conn, w->id, XCB_CW_EVENT_MASK, (uint32_t[]){ XCB_EVENT_MASK_ENTER_WINDOW });
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, w->id);    

    w->geom.x = w->geom.y = -1;
    w->geom.w = w->geom.h = 0;
    w->geom.depth = 0;

    fill_geometry(w->id, &w->geom.x, &w->geom.y, &w->geom.w, &w->geom.h, &w->geom.depth);


    w->d = 0;

    if (w->geom.x < 1 && w->geom.y < 1) window_center(w);
    
    w->dlist = NULL;
    w->wlist = node;


    /*create the frame*/
    xcb_create_window(
        conn, XCB_COPY_FROM_PARENT, w->handlebar, 
        w->id, w->geom.x, w->geom.y, 
        w->geom.w, w->geom.h, 0, 
        XCB_WINDOW_CLASS_INPUT_OUTPUT, 
        XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT, 
        (uint32_t[]){ 1 }
    );

    /*change the parent of the window*/
    // xcb_reparent_window(conn, w->id, w->frame, 0, 0);


    w->state_mask =  conf.border ? CELLO_STATE_BORDER : CELLO_STATE_NORMAL;

    // printf("Win info:\n\t| id = %d\n\t| frame = %d\n", w->id, w->frame);

    xcb_flush(conn);

    return w;
}

// desktop
uint32_t cello_get_current_desktop(){
    uint32_t current = 0;
    if ( xcb_ewmh_get_current_desktop_reply(ewmh, xcb_ewmh_get_current_desktop(ewmh, 0), &current, NULL) )
        return current;

    CRITICAL("{!!} Could not get the desktop list\n");
    exit(1);
}

// desktop
void cello_add_window_to_desktop(struct window * w, uint32_t ds){
    if (!w) return;

    struct list * node;
    if (!( node = new_empty_node(&dslist[ds]) )) return;
    w->dlist = node;
    w->d = ds;
    node->gdata = (char *) w;

    // change window desktop in ewmh
    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, w->id,
        ewmh->_NET_WM_DESKTOP, XCB_ATOM_CARDINAL,
        32, 1, &ds
	);

    /*verify if window can stay mapped*/
    {
        uint32_t c = cello_get_current_desktop();
        struct window * fw = (struct window *) dslist[c]->gdata;
        
        if (ds != c || !fw  || fw->state_mask & CELLO_STATE_MONOCLE || fw->state_mask & CELLO_STATE_MAXIMIZE )
            xcb_unmap_window(conn, w->id);

    }

    window_decorate(w);
}

// desktop
void cello_add_window_to_current_desktop(struct window * w){
    cello_add_window_to_desktop(w, cello_get_current_desktop());
}



#undef X_CENTER
#undef Y_CENTER

#define each_window_in_ds(ds) \
    (node = dslist[ds]; node; node=node->next )
// window
void cello_maximize_window(struct window * w) {
    if (!w) return;
    /*if already maximized, ignore*/
    if (w->state_mask & CELLO_STATE_MAXIMIZE) return;

    struct list * node;
    uint32_t current;
    
    current = cello_get_current_desktop();
    /*cannot maximize a window out of the current desktop*/
    if (!dslist[current] || w->d != current) return;

    /*unmap other windows before maximize*/
    for each_window_in_ds(current) {
        struct window * data = (struct window *) node->gdata;
        if (data->id != w->id)
            xcb_unmap_window(conn, data->id);
    }

    /*save the original geometry only if the window is at normal state*/
    if (__HasMask__(w->state_mask, CELLO_STATE_NORMAL))
        w->orig = w->geom;

    /*unset borders*/
    __RemoveMask__(w->state_mask, CELLO_STATE_BORDER);

    /*configure window before maximize*/
    xcb_move_window(w, 0, 0);
    xcb_resize_window(w, root_screen->width_in_pixels, root_screen->height_in_pixels);

    /*set maximized state*/
    __SwitchMask__(w->state_mask, CELLO_STATE_NORMAL | CELLO_STATE_MONOCLE, CELLO_STATE_MAXIMIZE);

    /*set wm state fullscreen*/
    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, 
        w->id, ewmh->_NET_WM_STATE, 
        XCB_ATOM_ATOM, 32, 1, 
        &ewmh->_NET_WM_STATE_FULLSCREEN
    );

    /*update decoration*/
    window_decorate(w);

    move_to_head(&dslist[cello_get_current_desktop()], w->dlist);

    xcb_flush(conn);
}

// window
void cello_monocle_window(struct window * w) {
    if (!w) return;
    if (w->state_mask & CELLO_STATE_MONOCLE) return;

    struct list * node;
    uint32_t current;

    current = cello_get_current_desktop();
    /*cannot maximize a window out of the current desktop*/
    if (!dslist[current] || w->d != current) return;

    /*unmap other windows before maximize*/
    for each_window_in_ds(current) {
        struct window * data = (struct window *) node->gdata;
        if (data->id != w->id) {
            xcb_unmap_window(conn, data->id);
            // xcb_unmap_window(conn, data->frame);
        }
    }

    /*save the original geometry*/
    if (!(w->state_mask & CELLO_STATE_MAXIMIZE))
        w->orig = w->geom;

    w->geom.w = root_screen->width_in_pixels;
    w->geom.h = root_screen->height_in_pixels;
    if (root_screen->width_in_pixels > conf.monocle_gap)
        w->geom.w -= conf.monocle_gap;
    if (root_screen->height_in_pixels > conf.monocle_gap)
        w->geom.h -= conf.monocle_gap;
    
    uint32_t smask = w->state_mask;
    w->state_mask &= ~CELLO_STATE_MAXIMIZE;
    w->state_mask &= ~CELLO_STATE_MONOCLE;
    window_center(w);
    xcb_resize_window(w, w->geom.w, w->geom.h);
    w->state_mask = smask;

    /*set maximized state*/
    w->state_mask &= ~CELLO_STATE_MAXIMIZE;
    w->state_mask |= CELLO_STATE_MONOCLE;
    
    move_to_head(&dslist[cello_get_current_desktop()], w->dlist);
    xcb_flush(conn);
}

#undef each_window_in_ds

#define get(w, l) w = (struct window *) l->gdata
#define get_and_map(w,l)  get(w,l); xcb_map_window(conn, w->id); 
#define get_and_unmap(w,l) get(w,l); xcb_unmap_window(conn, w->id);
// window
void cello_unmaximize_window(struct window * w) {
    if (!w) return;
    
    uint32_t current;
    struct window * win;
    struct list * aux;
    

    /*reconfigure the state*/
    __RemoveMask__(w->state_mask, CELLO_STATE_MAXIMIZE | CELLO_STATE_MONOCLE);
    /*revert to the original mask values*/
    __AddMask__(w->state_mask, w->tmp_state_mask);

    w->geom = w->orig;
    xcb_move_window(w, w->geom.x, w->geom.y);
    xcb_resize_window(w, w->geom.w, w->geom.h);

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->id, ewmh->_NET_WM_STATE, XCB_ATOM_ATOM, 32, 0, NULL);

    current = cello_get_current_desktop();
    /*and map all windows*/
    for (aux = dslist[current]; aux; aux=aux->next){
        __Node2Window__(aux, win);
        if (win->id != w->id){
            xcb_map_window(conn, win->id);
            // xcb_map_window(conn, win->frame);
        }
    }

    window_decorate(w);
    xcb_flush(conn);
}

// desktop
void cello_goto_desktop(uint32_t d){
    uint32_t current = cello_get_current_desktop();
    if (d == current || d > MAX_DESKTOPS)
        return;

    struct window * w;
    struct list * aux;

    /*unmap all windows from the old desktop*/
    for (aux = dslist[current]; aux; aux=aux->next){
        get_and_unmap(w,aux);
    }

    /*then map again the windows on the current desktop*/
    if ((aux=dslist[d])) {
        get_and_map(w,aux);

        /*check if there are some maximized window
         *(maximized/monocle windows are supposed to be always at the first position)
         */
        if (w->state_mask & CELLO_STATE_MAXIMIZE || w->state_mask & CELLO_STATE_MONOCLE) goto end_map;
        /*continue mapping the windows*/
        else for (aux = aux->next; aux; aux = aux->next){
            get_and_map(w,aux);
        }
    }
    end_map:
    ewmh_change_to_desktop(0, d);
    xcb_unfocus();

    xcb_query_pointer_reply_t * rpointer = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root_screen->root), NULL);
    if (rpointer) {
        xcb_focus_window(cello_find_window(rpointer->child));
        free(rpointer);
    }

    xcb_flush(conn);
}
#undef get_and_unmap
#undef get_and_map
#undef get

// window
void cello_update_wilist_with(xcb_drawable_t wid) {
    xcb_change_property(
        conn, XCB_PROP_MODE_APPEND , root_screen->root,
        ewmh->_NET_CLIENT_LIST, XCB_ATOM_WINDOW,
        32, 1, &wid
    );
    xcb_change_property(
        conn, XCB_PROP_MODE_APPEND , root_screen->root,
        ewmh->_NET_CLIENT_LIST_STACKING, XCB_ATOM_WINDOW,
        32, 1, &wid
    );
}

// window
void cello_update_wilist() {
    xcb_query_tree_cookie_t tcookie = xcb_query_tree(conn, root_screen->root);
	xcb_query_tree_reply_t *reply = xcb_query_tree_reply(conn, tcookie, 0);

	xcb_delete_property(conn, root_screen->root, ewmh->_NET_CLIENT_LIST);
	xcb_delete_property(conn, root_screen->root, ewmh->_NET_CLIENT_LIST_STACKING);

	if (!reply) {
		cello_update_wilist_with(0);
		return;
	}

	int tlen = xcb_query_tree_children_length(reply);
	xcb_window_t * tch = xcb_query_tree_children(reply);

	struct window *w;
    int i;

	for (i=0; i < tlen; i ++)
		if(( w = cello_find_window(tch[i]) ))
			cello_update_wilist_with(w->id);

	free(reply);
}

// window
void cello_unmap_win_from_desktop(struct window * w){
    if (!w) return;

    struct window * focused = xcb_get_focused_window();
    if (focused && focused->id == w->id) xcb_unfocus();
    pop_node(&dslist[w->d], w->dlist);

    w->d = -1;
    w->dlist = NULL;

    xcb_unmap_window(conn, w->id);
    // xcb_unmap_window(conn, w->frame);

    cello_update_wilist();
}

// xcb
void cello_unmap_window(struct window * w) {
    if(!w) return;

    cello_unmap_win_from_desktop(w);
    free_node(&wilist, w->wlist);
}

#define each_window (window=wilist;window;window=window->next)
// xcb
void cello_destroy_window(struct window * w){
    struct list * window;
    for each_window {
        struct window * win = (struct window *) window->gdata;
        if (win->id == w->id){
            cello_unmap_window(w);
            return;
        }
    }
}
#undef each_window

// window

void cello_grab_keys() {

    /*ungrab any key that for some reason was grabbed*/
    xcb_ungrab_key(conn, XCB_GRAB_ANY, root_screen->root, XCB_MOD_MASK_ANY);
    unsigned int i, j;
    const unsigned int keys_len =sizeof(keys) / sizeof(*keys);
    for (i = 0; i < keys_len; i++) {
        xcb_keycode_t * keycode = xcb_get_keycode_from_keysym(keys[i].key);

        for (j = 0; keycode[j]; j++){
            xcb_grab_key(
                conn, true, root_screen->root,
                keys[i].mod_mask, keycode[j],
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
            );
            xcb_grab_key(
                conn, true, root_screen->root,
                keys[i].mod_mask|XCB_MOD_MASK_LOCK, keycode[j],
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
            );
        }

        free(keycode);
    }
}

// atoms
void cello_init_atoms() {
    /*only one atom for now*/
    xcb_intern_atom_cookie_t catom = xcb_intern_atom(conn, false, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *ratom = xcb_intern_atom_reply(conn, catom, NULL);

    if (!ratom)
        WM_DELETE_WINDOW = 0;

    WM_DELETE_WINDOW = ratom->atom;
    free(ratom);
}

// remove
void cello_ewmh_init() {
    ewmh_init(conn);
    cello_init_atoms();
}

// confparse

/* Return true if the thread could start, 
 * so we can handle the pthread_join.
 * The config file must be passed so it
 * can be freed posteriorly with pthread_join.
 */
bool cello_read_config_file(pthread_t * config_thread, char * config_file) {
    if (!config_thread) return false;
    conf.config_ok = false;

    uint8_t baselen;

    config_file = ucalloc(25, sizeof(*config_file));
    strncpy(config_file, getenv("HOME"), 25);
    /* --- reallocate the config file to fit it's real size */
    baselen = strlen(config_file);
    if (baselen < 3) {
        if (config_file) free(config_file);
        return false;
    }
    config_file = urealloc(config_file, CONFIG_PATH_LEN + baselen);
    /* --- concat the config path */
    strncat(config_file, CONFIG_PATH, CONFIG_PATH_LEN);

    NLOG("{@} Looking for %s\n", config_file);
    /*check if file exists and can be read*/
    if (access(config_file,  F_OK|R_OK) != -1) {
        pthread_create(config_thread, NULL, (void *(*)(void*))&parse_json_config, config_file);
        return true;
    }
    ELOG("{@} Configuration file not found\n");
    free(config_file);
    return false;
}

void cello_reload() {
    cello_clean();

    /* dev reload can reload the execution, so we can test changes on each compile */
    #ifndef DEVRELOAD
        if (cello_setup_all()) cello_deploy();
    #else

    cello_clean();

    extern char * execpath;
    execvp(execpath, (char* []){execpath, NULL});
#endif
}

bool cello_setup_all() {
    pthread_t config_thread;
    char * config_file;
    bool config_open;


    int scr;
    static int current_ds = 0;

    NLOG("{@} Initializing %s\n", WMNAME);
    atexit(cello_exit);

    config_file = NULL;
    /*get the file status*/
    config_open = cello_read_config_file(&config_thread, config_file);

    if ( (scr = cello_setup_conn()) < 0 ) return false;

    if ( !( root_screen = xcb_get_root_screen(conn, scr)) ) {
        ELOG("{!} Setup failed\n");
        return false;
    }

    NLOG("{@} Screen dimensions %dx%d\n", root_screen->width_in_pixels, root_screen->height_in_pixels);

    cello_ewmh_init();

    ewmh_create_desktops(scr, MAX_DESKTOPS);
    ewmh_create_ewmh_window(conn, scr);

    if (current_ds)
        cello_goto_desktop(current_ds);
    NLOG("{@} Initializing %s\n", WMNAME);

    cello_grab_keys();

    cello_change_root_default_attributes();
    cello_setup_cursor();

    // wait for the configuration before setup the windows
    if (config_open) {
        pthread_join(config_thread, NULL);
        free(config_file);
    }


    xcb_grab_server(conn);
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            free(event);
            continue;
        }

        int type = (event->response_type & 0x7F);

        if (type == XCB_MAP_REQUEST)
            handle(event);

        free(event);
    }
    window_hijack();
    xcb_ungrab_server(conn);

    NLOG("{@} Setup completed!\n");
    return true;
}

void cello_deploy() {
    NLOG("{@} Deploying %s\n", WMNAME);
    handle_events();
}
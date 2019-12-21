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
// uint32_t current_ds = 0;

struct list * wilist;
struct list * dslist[MAX_DESKTOPS];

struct config conf;

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
    cello_clean();
    xcb_disconnect(conn);
}

static xcb_screen_t * get_cello_root_screen(int scr) {
    NLOG("{@} Looking for the root screen\n");

    xcb_screen_t * s = xcb_get_root_screen(conn, scr);

    #ifndef NO_DEBUG
    if (!s)
        ELOG("{!} Could not find the root screen\n");
    #endif

    return s;
}

__always_inline
static void cello_setup_cursor() {
    init_cursors();
    dynamic_cursor_set(CURSOR_POINTER);
}

static void cello_change_root_default_attributes() {
    (void) xcb_change_window_attributes(
        conn, root_screen->root, XCB_CW_EVENT_MASK, (uint32_t[1]) { ROOT_EVENT_MASK }
    );
}

struct window * cello_find_window(xcb_drawable_t wid){
    struct window * win;
    struct list * list;

    for (list = wilist; list; list=list->next){
        win = (struct window *) list->gdata;
        if (win->id == wid)
            return win;
    }

    return NULL;
}

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

uint32_t cello_get_win_desktop(xcb_window_t win){
    xcb_get_property_cookie_t cprop = xcb_get_property(
        conn, false, win, 
        ewmh->_NET_WM_DESKTOP,
        XCB_GET_PROPERTY_TYPE_ANY,
        false, sizeof(uint32_t)
    );
    uint32_t ds = 0;

    xcb_get_property_reply_t *rprop = xcb_get_property_reply(conn, cprop, NULL);
    if (!rprop || 0 == xcb_get_property_value_length(rprop)) { 
        ds = 0;
    } else {
        ds = *(uint32_t *)xcb_get_property_value(rprop);
    }
    if(rprop) free(rprop);

    return ds;
}

struct window * cello_configure_new_window(xcb_window_t win) {
    struct window * w;
    struct list * node;

    if (ewmh_is_special_window(win)) {
        xcb_map_window(conn, win);
        return NULL;
    }

    xcb_change_window_attributes_checked(conn, win, XCB_CW_EVENT_MASK, (uint32_t[]){ XCB_EVENT_MASK_ENTER_WINDOW });
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, win);
    if (!( node = new_empty_node(&wilist) )) return NULL;

    w = umalloc(sizeof(struct window));
    node->gdata = (char *) w;

    w->id = win;

    w->x = w->y = -1;
    w->w = w->h = 0;
    w->depth = 0;

    fill_geometry(win, &w->x, &w->y, &w->w, &w->h, &w->depth);

    // center the window if the position is not given
    if (w->x <= 0 && w->y <= 0)
        cello_center_window(w);

    w->d = -1;
    w->dlist = NULL;
    w->wlist = node;

    w->deco_mask =  conf.border ? DECO_BORDER : DECO_NO_BORDER;

    return w;    
}

uint32_t cello_get_current_desktop(){
    uint32_t current = 0;
    if ( xcb_ewmh_get_current_desktop_reply(ewmh, xcb_ewmh_get_current_desktop(ewmh, 0), &current, NULL) )
        return current;

    CRITICAL("{!!} Could not get the desktop list\n");
    exit(1);
}

#define INNER \
        { w->w, 0, ibw, w->h+ibw },          \
        { w->w+bw+obw, 0, ibw, w->h+ibw }, \
		{ 0, w->h+bw+obw, w->w+ibw, ibw },          \
        { 0, w->h, w->w+ibw, ibw },      \
        { w->w+bw+obw, w->h+bw+obw,  bw, bw}

#define OUTER \
		{ w->w+ibw, 0, obw, w->h+bw*2 },         \
		{ w->w+bw, 0, obw, w->h+bw*2 },         \
		{ 0, w->h+ibw, w->w+bw*2, obw },          \
		{ 0, w->h+bw, w->w+bw*2, obw },          \
        { 1,1,1,1 }

void cello_decorate_window(struct window * w){
    if (w->deco_mask & DECO_NO_BORDER) {
        xcb_configure_window(conn, w->id, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){0});
        return;
    };

	uint32_t values[1];

    uint16_t obw = conf.outer_border;
    uint16_t ibw = conf.inner_border;
    uint16_t bw = ibw+obw;

    // printf("%d+-+%d+-+%d\n", obw, ibw, bw);

	*values = bw;
	xcb_configure_window(conn, w->id,
			XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

	xcb_rectangle_t rect_inner[] = {
		INNER
	};

	xcb_rectangle_t rect_outer[] = {
		OUTER
	};

	xcb_pixmap_t pmap = xcb_generate_id(conn);
	xcb_create_pixmap(conn, w->depth, pmap, w->id,
			w->w + (bw*2),
			w->h + (bw*2)
	);

	xcb_gcontext_t gc = xcb_generate_id(conn);
	xcb_create_gc(conn, gc, pmap, 0, NULL);


	*values = conf.outer_border_color;

	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &*values);
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_outer);


	*values = conf.inner_border_color;

	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &*values);
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_inner);
    
	values[0] = pmap;
	xcb_change_window_attributes(conn,w->id, XCB_CW_BORDER_PIXMAP,
			&*values);

	xcb_free_pixmap(conn,pmap);
	xcb_free_gc(conn,gc);
	xcb_flush(conn);
}

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

    cello_decorate_window(w);

    if (ds != cello_get_current_desktop())
        xcb_unmap_window(conn, w->id);
}

void cello_add_window_to_current_desktop(struct window * w){
    cello_add_window_to_desktop(w, cello_get_current_desktop());
}

void cello_move_window(xcb_window_t w, int16_t x, int16_t y){
	if (w == root_screen->root || w == 0) return;
    
	xcb_configure_window(conn, w,  XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (uint32_t[]){x, y});
	xcb_flush(conn);
}

#define X_CENTER abs((root_screen->width_in_pixels - w->w))/2
#define Y_CENTER abs((root_screen->height_in_pixels - w->h))/2

void cello_center_window(struct window * w) {
    if (!w) return;
    w->x = X_CENTER;
    w->y = Y_CENTER;
    cello_move_window(w->id, w->x, w->y);
}

void cello_center_window_x(struct window * w) {
    if (!w) return;
    w->x = X_CENTER;
    cello_move_window(w->id, w->x, w->y);
}

void cello_center_window_y(struct window * w) {
    if (!w) return;
    w->y = Y_CENTER;
    cello_move_window(w->id, w->x, w->y);
}

#undef X_CENTER
#undef Y_CENTER

void cello_goto_desktop(uint32_t d){
    uint32_t current = cello_get_current_desktop();
    if (d == current || d > MAX_DESKTOPS)
        return;

    struct window * w;
    struct list * aux;

    for (aux = dslist[current]; aux; aux=aux->next){
        w = (struct window *) aux->gdata;
        xcb_unmap_window(conn, w->id);
    }
    for (aux = dslist[d]; aux; aux = aux->next){
        w = (struct window *) aux->gdata;
        xcb_map_window(conn, w->id);
    }

    ewmh_change_to_desktop(0, d);

    xcb_query_pointer_reply_t * rpointer = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root_screen->root), NULL);
    if (!rpointer) xcb_unfocus();
    else {
        xcb_focus_window(cello_find_window(rpointer->child));
        free(rpointer);
    }
}

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

void cello_unmap_win_from_desktop(struct window * w){
    if (!w) return;

    struct window * focused = xcb_get_focused_window();
    if (focused && focused->id == w->id) xcb_unfocus();

    pop_node(&dslist[w->d], w->dlist);

    w->d = -1;
    w->dlist = NULL;

    xcb_unmap_window(conn, w->id);
    cello_update_wilist();
}

void cello_unmap_window(struct window * w) {
    if(!w) return;

    cello_unmap_win_from_desktop(w);
    free_node(&wilist, w->wlist);
}

#define each_window (window=wilist;window;window=window->next)
void cello_destroy_window(xcb_window_t win){
    struct list * window;
    for each_window {
        struct window * w = (struct window *) window->gdata;
        if (w->id == win){
            cello_unmap_window(w);
            return;
        }
    }
}
#undef each_window


void cello_setup_windows(){
    xcb_query_tree_reply_t * rtree;
    xcb_query_tree_cookie_t ctree;
    xcb_window_t * children;

    xcb_get_window_attributes_reply_t * rattr;
    xcb_get_window_attributes_cookie_t cattr;

    struct window * w;
    
    cello_update_wilist();
    
    ctree = xcb_query_tree(conn, root_screen->root);
    if ((rtree = xcb_query_tree_reply(conn, ctree, NULL)) == NULL) {
        CRITICAL("Could not get query tree");
    }

    const unsigned int len = xcb_query_tree_children_length(rtree);
    children = xcb_query_tree_children(rtree);

    unsigned int i;
    for (i = 0; i < len; i++) {
        cattr = xcb_get_window_attributes(conn, children[i]);
        if ((rattr = xcb_get_window_attributes_reply(conn, cattr, NULL)) == NULL)
            continue;

        if (!rattr->override_redirect && rattr->map_state == XCB_MAP_STATE_VIEWABLE) {
            w = cello_configure_new_window(children[i]);
            if (w) {
                if (!wilist) xcb_focus_window(w);
                uint32_t ds = cello_get_win_desktop(w->id);
                cello_decorate_window(w);
                cello_add_window_to_desktop(w, ds);
                cello_update_wilist_with(children[i]);
            }
        }
    }

    if (dslist[cello_get_current_desktop()])
        xcb_focus_window((struct window *)dslist[cello_get_current_desktop()]->gdata);
}

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

void cello_init_atoms() {
    /*only one atom for now*/
    xcb_intern_atom_cookie_t catom = xcb_intern_atom(conn, false, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *ratom = xcb_intern_atom_reply(conn, catom, NULL);

    if (!ratom)
        WM_DELETE_WINDOW = 0;

    WM_DELETE_WINDOW = ratom->atom;
    free(ratom);
}

void cello_ewmh_init() {
    ewmh_init(conn);
    cello_init_atoms();
}

bool cello_read_config_file(pthread_t * config_thread, char * config_file) {
    if (!config_thread) return false;

    strncat(config_file, getenv("HOME"), 25);

    if (config_file) {
        config_file = urealloc(config_file, CONFIG_PATH_LEN + strlen(config_file));
        strcat(config_file, CONFIG_PATH);

        ELOG("{@} Looking for %s\n", config_file);

        if (access(config_file,  F_OK|R_OK) != -1) {
            pthread_create(config_thread, NULL, (void *(*)(void*))&parse_json_config, config_file);
            return true;
        }
        else
            return (conf.config_ok = false);
    }
    return false;
}

void cello_reload() {
    // current_ds = cello_get_current_desktop();

    cello_clean();
    
/* dev reload can reload the execution, so we can test changes on each compile */
#ifndef DEVRELOAD
    if (cello_setup_all()) cello_deploy();
#else
    extern char * execpath;
    execvp(execpath, (char* []){execpath, NULL});
#endif
}

bool cello_setup_all() {
    NLOG("{@} Initializing %s\n", WMNAME);

    atexit(cello_exit);

    int scr;

    pthread_t config_thread;
    char * config_file;
    bool config_open = false;

    // initial size to store the home path
    config_file = ucalloc(35, sizeof(char));
    config_open = cello_read_config_file(&config_thread, config_file);

    if ( (scr = cello_setup_conn()) < 0 ) return false;

    if ( !( root_screen = get_cello_root_screen(scr)) ) {
        ELOG("{!} Setup failed\n");
        return false;
    }
    
    NLOG("{@} Screen dimensions %dx%d\n", root_screen->width_in_pixels, root_screen->height_in_pixels);

    cello_ewmh_init(conn);

    ewmh_create_desktops(scr, MAX_DESKTOPS);
    ewmh_create_ewmh_window(conn, scr);

    if (0/*current_ds*/)
        cello_goto_desktop(0/*current_ds*/);

    cello_grab_keys();

    cello_change_root_default_attributes();
    cello_setup_cursor();
    
    // load the configuration before setup the windows
    if (config_open) pthread_join(config_thread, NULL);
    if (*config_file) free(config_file);

    cello_setup_windows();

    NLOG("{@} Setup completed!\n");
    return true;
}

void cello_deploy() {
    NLOG("{@} Deploying %s\n", WMNAME);
    handle_events();
}

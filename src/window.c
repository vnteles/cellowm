#include "window.h"

#include <xcb/shape.h>

#include "cello.h"

#include "ewmh.h"
#include "list.h"
#include "utils.h"
#include "xcb.h"
#include "log.h"


struct window_list *wilist;

static struct window * find_window(xcb_drawable_t id, uint8_t byid) {
    struct window *w;
    struct window_list *list;

    for (list = wilist; list; list = list->next) {
        w = (struct window *)list->window;

        if ((byid && w->id == id) || (!byid && w->frame == id)){
            return w;
        }

    }

    return NULL;
}

struct window * find_window_by_id(xcb_drawable_t wid) {
    return find_window(wid, true);
}

struct window * find_window_by_frame(xcb_drawable_t fid) {
    return find_window(fid, false);
}

static struct geometry get_geometry(xcb_drawable_t wid) {
    struct geometry geometry;
    geometry.x = geometry.y = geometry.w = geometry.h = geometry.depth = 0;

    xcb_get_geometry_cookie_t geometry_cookie;
    geometry_cookie = xcb_get_geometry(conn, wid);

    // get the geometry reply
    xcb_get_geometry_reply_t * geometry_reply;
    if (!(geometry_reply = xcb_get_geometry_reply(conn, geometry_cookie, NULL)))
        return geometry;

    // set the geometry
    geometry.x = geometry_reply->x;
    geometry.y = geometry_reply->y;
    geometry.w = geometry_reply->width;
    geometry.h = geometry_reply->height;
    geometry.depth = geometry_reply->depth;

    return geometry;
}

void generate_frame(struct window * w) {
    w->frame = xcb_generate_id(conn);

    xcb_create_window(conn, root_screen->root_depth,
        w->frame, root_screen->root, w->geom.x,
        w->geom.y, w->geom.w, w->geom.h, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        root_screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
        (uint32_t[]){
            0xff00ff00,
            false,
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_ENTER_WINDOW
        }
    );

    xcb_change_save_set(conn, XCB_CHANGE_SAVE_SET, w->frame);
    // DLOG("Frame %0x%X generated for window 0x%X\n", w->frame, w->id);
    xcb_reparent_window(conn, w->id, w->frame, 0, 0);

}

struct window *window_configure_new(xcb_window_t win) {
    struct window *w;
    struct window_list *node;

    bool has_maximized = false;
    uint32_t current_ds;

    ewmh_handle_strut(win);

    if (ewmh_is_special_window(win)) {
        xcb_map_window(conn, win);
        return NULL;
    }

    w = umalloc(sizeof(window));

    current_ds = cello_get_current_desktop();
    if (dslist[current_ds] && !(dslist[current_ds]->window->state_mask | CELLO_STATE_NORMAL))
        has_maximized = true;

    if (!(node = new_empty_node(&wilist)))
        return 0;

    if (has_maximized)
        bring_to_head(&dslist[current_ds], &dslist[current_ds][1]);

    node->window = w;

    w->id = win;

    // xcb_change_window_attributes_checked(
    //     conn, w->id, XCB_CW_EVENT_MASK,
    //     (uint32_t[]){XCB_EVENT_MASK_ENTER_WINDOW}
    // );

    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, w->id);

    w->geom.x = w->geom.y = -1;
    w->geom.w = w->geom.h = w->geom.depth = 0;

    w->geom = get_geometry(w->id);

    w->d = 0;

    w->dlist = NULL;
    w->wlist = node;

    w->state_mask = CELLO_STATE_NORMAL;
    w->state_mask |= conf.border ? CELLO_STATE_BORDER : 0;
    if (w->geom.x < 1 && w->geom.y < 1)
        center_window(w);

    generate_frame(w);

    xcb_flush(conn);
    return w;
}

#define X_CENTER abs((root_screen->width_in_pixels - w->geom.w)) / 2
#define Y_CENTER abs((root_screen->height_in_pixels - w->geom.h)) / 2

void center_window(struct window *w) {
    if (!w)
        return;
    xcb_move_window(w, X_CENTER, Y_CENTER);
}

void center_window_x(struct window *w) {
    if (!w)
        return;
    xcb_move_window(w, X_CENTER, w->geom.y);
}

void center_window_y(struct window *w) {
    if (!w)
        return;
    xcb_move_window(w, w->geom.x, Y_CENTER);
}

#undef X_CENTER
#undef Y_CENTER

void update_decoration(struct window *w) {
    /* ignore if no border modifier is specified */
    if (!(w->state_mask & CELLO_STATE_BORDER)) {
        xcb_configure_window(conn, w->id, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){0});
        return;
    };

    uint16_t obw = conf.outer_border;
    uint16_t ibw = conf.inner_border;
    uint16_t bw = ibw + obw;

    xcb_configure_window(conn, w->id, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){bw});

    /*generate the pixmap*/
    xcb_pixmap_t pmap = xcb_generate_id(conn);
    xcb_create_pixmap(
        conn, w->geom.depth, pmap, w->id, w->geom.w + (bw * 2), w->geom.h + (bw * 2)
    );

    /*generate the graphic context*/
    xcb_gcontext_t gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, pmap, 0, NULL);

    /*rectangle definitions*/
#define __InnerBorder__                                                 \
    (xcb_rectangle_t[]) {                                               \
        {w->geom.w, 0, ibw, w->geom.h + ibw},                           \
        {w->geom.w + bw + obw, 0, ibw, w->geom.h + ibw},                \
        {0, w->geom.h + bw + obw, w->geom.w + ibw, ibw},                \
        {0, w->geom.h, w->geom.w + ibw, ibw},                           \
        { w->geom.w + bw + obw, w->geom.h + bw + obw, bw, bw }          \
    }

#define __OuterBorder__                                                 \
    (xcb_rectangle_t[]) {                                               \
        {w->geom.w + ibw, 0, obw, w->geom.h + bw * 2},                  \
        {w->geom.w + bw, 0, obw, w->geom.h + bw * 2},                   \
        {0, w->geom.h + ibw, w->geom.w + bw * 2, obw},                  \
        {0, w->geom.h + bw, w->geom.w + bw * 2, obw},                   \
    }

    /*draw the outer border*/
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){conf.outer_border_color | 0xff000000});
    xcb_poly_fill_rectangle(conn, pmap, gc, 4, __OuterBorder__);

    /*draw the inner border*/
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){conf.inner_border_color | 0xff000000});
    xcb_poly_fill_rectangle(conn, pmap, gc, 5, __InnerBorder__);

#undef __OuterBorder__
#undef __InnerBorder__

    xcb_change_window_attributes(conn, w->id, XCB_CW_BORDER_PIXMAP,
                               (uint32_t[]){pmap});

    (void)xcb_free_pixmap(conn, pmap);
    (void)xcb_free_gc(conn, gc);
    (void)xcb_flush(conn);
}

uint32_t get_window_desktop(xcb_window_t win) {
    uint32_t ds;
    xcb_get_property_cookie_t cprop;
    xcb_get_property_reply_t *rprop;

    cprop = xcb_get_property(
        conn, false, win, ewmh->_NET_WM_DESKTOP,
        XCB_GET_PROPERTY_TYPE_ANY, false, sizeof(uint32_t)
    );

    ds = 0;

    rprop = xcb_get_property_reply(conn, cprop, NULL);

    if (!rprop || !xcb_get_property_value_length(rprop))
        ds = 0;
    else
        ds = *(uint32_t *)xcb_get_property_value(rprop);

    if (rprop)
        ufree(rprop);

    return ds;
}

void window_hijack() {

    xcb_query_tree_reply_t *rtree;
    xcb_query_tree_cookie_t ctree;
    xcb_window_t *children;

    xcb_get_window_attributes_reply_t *rattr;
    xcb_get_window_attributes_cookie_t cattr;

    struct window *w;

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

            w = window_configure_new(children[i]);

            if (w) {
                if (!dslist[cello_get_current_desktop()])
                    xcb_focus_window(w);

                /*add window and draw the frame*/
                cello_add_window_to_desktop(w, get_window_desktop(w->id));
                xcb_map_window(conn, w->frame);
                // xcb_map_window(conn, w->frame);

                update_decoration(w);
                // XCB_CW_

                cello_update_wilist_with(w->id);
                puts("------new window added");
            }
        }
    }

    if (dslist[cello_get_current_desktop()])
        xcb_focus_window(
                (struct window *)dslist[cello_get_current_desktop()]->window);

    NLOG("All windows hijacked.");
    xcb_flush(conn);
}

#define each_window_in_ds(ds) (node = dslist[ds]; node; node = node->next)

struct window * nextprev_window(bool prev) {
    struct window * focused;

    struct window_list * wlhead = dslist[cello_get_current_desktop()];
    focused = xcb_get_focused_window();

    for (struct window_list * wl = wlhead; wl && wl->window; wl=wl->next) {
        struct window * w = (struct window *) wl->window;

        if (w->id == focused->id) {
            if (prev) {
                wl=wl->prev;
            } else {
                wl=wl->next;
            }

            if (wl && wl->window) {
                w = (struct window *) wl->window;
            }

            return w;
        }

    }
    return NULL;
}

void window_maximize(struct window *w, uint16_t stt) {
    uint32_t cds;

    if (!w)
        return;
    /*if already maximized, ignore*/
    if (w->state_mask & stt)
        return;

    cds = cello_get_current_desktop();

    /*cannot maximize a window out of the current desktop*/
    if (!dslist[cds] || w->d != cds)
        return;

    /*unmap all the windows before maximize*/
    unmap_desktop(cds);
    /*map our window again*/
    xcb_map_window(conn, w->id);

    int16_t
        move_x = 0,
        move_y = 0;

    uint16_t
        resize_w = root_screen->width_in_pixels,
        resize_h = root_screen->height_in_pixels;


    if (stt & CELLO_STATE_FOCUS) {
        resize_w -= conf.focus_gap*2;
        resize_h -= conf.focus_gap*2;

        move_x += conf.focus_gap;
        move_y += conf.focus_gap;
    }

    /*save the original geometry only if the window is at normal state*/
    if (w->state_mask & CELLO_STATE_NORMAL) {
        w->orig = w->geom;
        w->tmp_state_mask = w->state_mask;
    }
    else {
        /*let window moveable*/
        __SwitchMask__(
            w->state_mask,
            CELLO_STATE_FOCUS | CELLO_STATE_MAXIMIZE,
            CELLO_STATE_NORMAL
        );
    }

    /*configure window before maximize*/
    xcb_move_window( w, move_x, move_y );
    xcb_resize_window( w, resize_w, resize_h );

    /*set maximized state*/
    __SwitchMask__(w->state_mask, CELLO_STATE_NORMAL, stt);

    /*set wm state to fullscreen*/
    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, w->id, ewmh->_NET_WM_STATE,
        XCB_ATOM_ATOM, 32, 1, &ewmh->_NET_WM_STATE_FULLSCREEN
    );

    /*update decoration*/
    update_decoration(w);

    bring_to_head(&dslist[cello_get_current_desktop()], w->dlist);

    xcb_flush(conn);
}
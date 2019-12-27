#include "window.h"

#include "cello.h"
#include "xcb.h"
#include "utils.h"
#include "ewmh.h"

#include "log.h"

#define X_CENTER abs((root_screen->width_in_pixels - w->geom.w)) / 2
#define Y_CENTER abs((root_screen->height_in_pixels - w->geom.h)) / 2

void window_center(struct window *w)
{
    if (!w)
        return;
    xcb_move_window(w, X_CENTER, Y_CENTER);
}

void window_center_x(struct window *w)
{
    if (!w)
        return;
    xcb_move_window(w, X_CENTER, w->geom.y);
}

void window_center_y(struct window *w)
{
    if (!w)
        return;
    xcb_move_window(w, w->geom.x, Y_CENTER);
}

#undef X_CENTER
#undef Y_CENTER

void window_decorate(struct window *w)
{
#define __InnerBorder__                                         \
    (xcb_rectangle_t[])                                         \
    {                                                           \
        {w->geom.w, 0, ibw, w->geom.h + ibw},                   \
        {w->geom.w + bw + obw, 0, ibw, w->geom.h + ibw},        \
        {0, w->geom.h + bw + obw, w->geom.w + ibw, ibw},        \
        {0, w->geom.h, w->geom.w + ibw, ibw},                   \
        {w->geom.w + bw + obw, w->geom.h + bw + obw, bw, bw}    \
    }

#define __OuterBorder__                                    \
    (xcb_rectangle_t[])                                    \
    {                                                      \
        {w->geom.w + ibw, 0, obw, w->geom.h + bw * 2},     \
        {w->geom.w + bw, 0, obw, w->geom.h + bw * 2},      \
        {0, w->geom.h + ibw, w->geom.w + bw * 2, obw},     \
        {0, w->geom.h + bw, w->geom.w + bw * 2, obw},      \
        {1, 1, 1, 1}                                       \
    }

    if (!(w->state_mask & CELLO_STATE_BORDER))
    {
        (void)xcb_configure_window(conn, w->id, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){0});
        return;
    };

    uint16_t obw = conf.outer_border;
    uint16_t ibw = conf.inner_border;
    uint16_t bw = ibw + obw;

    (void)xcb_configure_window(conn, w->id, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){bw});

    /*generate the pixmap*/
    xcb_pixmap_t pmap = xcb_generate_id(conn);
    (void)xcb_create_pixmap(conn, w->geom.depth, pmap, w->id,
                            w->geom.w + (bw * 2),
                            w->geom.h + (bw * 2));

    /*generate the graphic context*/
    xcb_gcontext_t gc = xcb_generate_id(conn);
    (void)xcb_create_gc(conn, gc, pmap, 0, NULL);

    /*draw the outer border*/
    (void)xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){conf.outer_border_color | 0xff000000});
    (void)xcb_poly_fill_rectangle(conn, pmap, gc, 5, __OuterBorder__);

    /*draw the inner border*/
    (void)xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){conf.inner_border_color | 0xff000000});
    (void)xcb_poly_fill_rectangle(conn, pmap, gc, 5, __InnerBorder__);

    (void)xcb_change_window_attributes(conn, w->id, XCB_CW_BORDER_PIXMAP, (uint32_t[]){pmap});

    (void)xcb_free_pixmap(conn, pmap);
    (void)xcb_free_gc(conn, gc);
    (void)xcb_flush(conn);

#undef __OuterBorder__
#undef __InnerBorder__
}

uint32_t get_window_desktop(xcb_window_t win){
    uint32_t ds;
    xcb_get_property_cookie_t cprop;
    xcb_get_property_reply_t * rprop;

    cprop = xcb_get_property(
        conn, false, win,
        ewmh->_NET_WM_DESKTOP,
        XCB_GET_PROPERTY_TYPE_ANY,
        false, sizeof(uint32_t)
    );

    ds = 0;

    rprop = xcb_get_property_reply(conn, cprop, NULL);

    if (!rprop || !xcb_get_property_value_length(rprop))
        ds = 0;
    else
        ds = *(uint32_t *) xcb_get_property_value(rprop);

    if(rprop) ufree(rprop);

    return ds;
}

void window_hijack()
{
    xcb_query_tree_reply_t *rtree;
    xcb_query_tree_cookie_t ctree;
    xcb_window_t *children;

    xcb_get_window_attributes_reply_t *rattr;
    xcb_get_window_attributes_cookie_t cattr;

    struct window *w;

    cello_update_wilist();

    ctree = xcb_query_tree(conn, root_screen->root);
    if ((rtree = xcb_query_tree_reply(conn, ctree, NULL)) == NULL)
    {
        CRITICAL("Could not get query tree");
    }

    const unsigned int len = xcb_query_tree_children_length(rtree);
    children = xcb_query_tree_children(rtree);

    unsigned int i;
    for (i = 0; i < len; i++)
    {
        cattr = xcb_get_window_attributes(conn, children[i]);
        if ((rattr = xcb_get_window_attributes_reply(conn, cattr, NULL)) == NULL)
            continue;

        if (!rattr->override_redirect && rattr->map_state == XCB_MAP_STATE_VIEWABLE)
        {
            w = cello_configure_new_window(children[i]);
            if (w)
            {
                if (!dslist[cello_get_current_desktop()])
                    xcb_focus_window(w);

                /*add window and draw the frame*/
                cello_add_window_to_desktop(w, get_window_desktop(w->id));
                // xcb_map_window(conn, w->frame);

                window_decorate(w);

                cello_update_wilist_with(w->id);
            }
        }
    }

    if (dslist[cello_get_current_desktop()])
        xcb_focus_window((struct window *)dslist[cello_get_current_desktop()]->gdata);

    xcb_flush(conn);
}

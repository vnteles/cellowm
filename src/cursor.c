#include "utils.h"
#include "cursor.h"
#include "cello.h"
#include "log.h"

#include <assert.h>
#include <xcb/xcb_cursor.h>

static const xcb_cursor_t xcb_cursors[MAX_CURSOR] = {
    [CURSOR_POINTER]            =   FALLBACK_CURSOR_LEFT_PTR,
    [CURSOR_RESIZE_HORIZONTAL]  =   FALLBACK_CURSOR_SB_H_DOUBLE_ARROW,
    [CURSOR_RESIZE_VERTICAL]    =   FALLBACK_CURSOR_SB_V_DOUBLE_ARROW,
    [CURSOR_WATCH]              =   FALLBACK_CURSOR_WATCH,
    [CURSOR_MOVE]               =   FALLBACK_CURSOR_FLEUR
};


static xcb_cursor_t cursors[MAX_CURSOR];
static bool suport_xcursor = false;

#define set(index, name) { \
    cursors[index] = xcb_cursor_load_cursor(ctx, name); \
}
void init_cursors(){
    xcb_cursor_context_t * ctx;

    if (xcb_cursor_context_new(conn, root_screen, &ctx) < 0) {
        suport_xcursor = false;
        return;
    }

    set(CURSOR_POINTER, "left_ptr");
    set(CURSOR_RESIZE_HORIZONTAL, "sb_h_double_arrow");
    set(CURSOR_RESIZE_VERTICAL, "sb_v_double_arrow");
    set(CURSOR_SIZING, "sizing");
    set(CURSOR_WATCH, "watch");
    set(CURSOR_MOVE, "fleur");
    set(CURSOR_TOP_LEFT_CORNER, "top_left_corner");
    set(CURSOR_TOP_RIGHT_CORNER, "top_right_corner");
    set(CURSOR_BOTTOM_LEFT_CORNER, "bottom_left_corner");
    set(CURSOR_BOTTOM_RIGHT_CORNER, "bottom_right_corner");

    suport_xcursor = true;
}
#undef set

bool check_xcursor_support() {
    return suport_xcursor;
}


xcb_cursor_t get_cursor(uint8_t id) {
    assert(id < MAX_CURSOR);
    return cursors[id];
}

uint8_t get_fallback_cursor_code(uint8_t cursor) {
    assert(cursor < sizeof(xcb_cursors)/sizeof(*xcb_cursors));
    return xcb_cursors[cursor];
}

xcb_cursor_t get_fallback_cursor(uint8_t id){
    xcb_cursor_t xcb_cursor = xcb_generate_id(conn);
    xcb_font_t xcb_font = xcb_generate_id(conn);

    (void) xcb_open_font(conn, xcb_font, 6, "cursor");

    uint32_t cursor_code = get_fallback_cursor_code(id);

    (void) xcb_create_glyph_cursor(
        conn,     xcb_cursor,   xcb_font,
        xcb_font, cursor_code, cursor_code + 1,
        00000,    00000,        00000,
        65535,    65535,        65535
    );

    (void) xcb_close_font(conn, xcb_font);
    return xcb_cursor;
}

void set_fallback_cursor(uint8_t cursor) {
    NLOG("Setting up the XCB fallback cursors");

    xcb_cursor_t xcb_cursor = get_fallback_cursor(cursor);

    (void) xcb_change_window_attributes(conn, root_screen->root, XCB_CW_CURSOR, &xcb_cursor);
    (void) xcb_free_cursor(conn, xcb_cursor);

    (void) xcb_flush(conn);
}

void set_cursor(CCursor cursor_id){
    NLOG("Setting up the XCursors");
    xcb_cursor_t c =  get_cursor((uint8_t) cursor_id);
    (void) xcb_change_window_attributes(conn, root_screen->root, XCB_CW_CURSOR, (uint32_t[]){
        c
    });
    // xcb_free_cursor(conn, c);
    (void) xcb_flush(conn);
}

void dynamic_cursor_set(CCursor cur){
    if (check_xcursor_support())
        set_cursor(cur);
    else
        set_fallback_cursor(cur);
}

xcb_cursor_t dynamic_cursor_get(uint8_t id) {
    if (suport_xcursor)
        return get_cursor(id);
    else
        return get_fallback_cursor(id);
}
#pragma once

#include <xcb/xcb_cursor.h>
#include "utils.h"

/* CCursor indexes */
typedef enum cursor { 
    CURSOR_POINTER = 0,
    CURSOR_RESIZE_HORIZONTAL,
    CURSOR_RESIZE_VERTICAL,
    CURSOR_TOP_LEFT_CORNER,
    CURSOR_TOP_RIGHT_CORNER,
    CURSOR_BOTTOM_LEFT_CORNER,
    CURSOR_BOTTOM_RIGHT_CORNER,
    CURSOR_SIZING,
    CURSOR_WATCH,
    CURSOR_MOVE,
    MAX_CURSOR,
} CCursor;

/* xcb cursor codes */
#define FALLBACK_CURSOR_FLEUR 52 /* Move */
#define FALLBACK_CURSOR_LEFT_PTR 68 /* Left pointer */
#define FALLBACK_CURSOR_SB_H_DOUBLE_ARROW 108 /* Horizontal resize */
#define FALLBACK_CURSOR_SB_V_DOUBLE_ARROW 116 /* Vertical resize */
#define FALLBACK_CURSOR_SIZING 120 /* Resize */
#define FALLBACK_CURSOR_WATCH 150 /* Clock */

/* init the xcursors array
 * return true if it supports xcursor
 */
void init_cursors();
/* check xcursor support */
bool check_xcursor_support();

/* set an xcb cursor */
void set_fallback_cursor(uint8_t cursor);

/*return the cursor code of the given cursor*/
uint8_t get_fallback_cursor_code(uint8_t cursor);
/* get xcb cursor */
xcb_cursor_t get_fallback_cursor(uint8_t id);


/* set an xcursor  */
void set_cursor(CCursor cursor);
/* get xcursor code */
xcb_cursor_t get_cursor(uint8_t id);

/* if xcursor not supported, set fallback, otherwise, set xcursor */
void dynamic_cursor_set(CCursor cur);
xcb_cursor_t dynamic_cursor_get(uint8_t id);
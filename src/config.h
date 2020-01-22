#pragma once

#include "utils.h"

#include "include/types.h"
#include "include/handlers.h"
#include "include/events.h"

#define command(com_name, com, ...) \
    __attribute_used__ \
    static char * com_name[] = { com, ##__VA_ARGS__, 0 };

#define CHANGEDESKTOP(k, n) \
    {XK_##k,MOD,CHANGE_DESKTOP,NONE,{.i = n}}, \
    {XK_##k,MOD|SHIFT,MOVE_WINDOW_TO_DESKTOP, NONE, {.i=n}}
    

#define SHIFT XCB_MOD_MASK_SHIFT
#define ALT XCB_MOD_MASK_1
#define CONTROL XCB_MOD_MASK_CONTROL

/* user changes here */
#define MOD XCB_MOD_MASK_4

command(terminal, "termite");
command(fixed_menu, "rofi", "-show", "drun");
command(context_menu, "jgmenu");
command(reload_polybar, "~/.config/polybar/launch.sh");
command(print, "scrot");

/*these ifndef are just for files that include config.h but dont use these configuration*/
#ifndef NO_BUTTONS

// button map | temporary?
static struct button buttons[] = {
    { XCB_BUTTON_INDEX_1,  MOD, on_mouse_motion, NO_ROOT, {.i = MOVE_WINDOW } },
    { XCB_BUTTON_INDEX_3,  MOD, on_mouse_motion, NO_ROOT, {.i = RESIZE_WINDOW} },

    /*feel free to change this*/
    // { XCB_BUTTON_INDEX_3,  MOD, RUN, ROOT_ONLY, { .com = context_menu } },
};

#endif

#ifndef NO_KEYS

#include <X11/keysymdef.h>
#include <X11/keysym.h>

/*user changes here*/
static struct key keys[] = {
    /* { key, modifier, function, window modifier, param [ config / command ]  */
    //reload wm
    { XK_r,                MOD|SHIFT,     RELOAD_CONFIG,   NONE,             {}                      },

    //center window
    { XK_c,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = NONE}           },
    //center window on x axis
    { XK_x,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = X}              },
    //center window on y axis
    { XK_y,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = Y}              },

    {XK_b,                 MOD,           TOGGLE_BORDER,   NO_ROOT,          {}                    },
    {XK_f,                 MOD,           TOGGLE_MAXIMIZE, NO_ROOT,          {}                    },
    {XK_m,                 MOD,           TOGGLE_FOCUS_MODE,  NO_ROOT,          {}                    },

    CHANGEDESKTOP(1, 0),
    CHANGEDESKTOP(2, 1),
    CHANGEDESKTOP(3, 2),
    CHANGEDESKTOP(4, 3),
    CHANGEDESKTOP(5, 4),
    CHANGEDESKTOP(6, 5),
    CHANGEDESKTOP(7, 6),
    CHANGEDESKTOP(8, 7),
    CHANGEDESKTOP(9, 8),
    CHANGEDESKTOP(0, 9),
};

#endif


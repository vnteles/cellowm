#pragma once

#include "utils.h"

#include "include/types.h"
#include "include/handlers.h"

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

static struct button buttons[] = {
    /* { button, modifier, function, window modifier, param [ config / command ]  */
    { XCB_BUTTON_INDEX_1,  MOD,           MOUSE_MOTION,    NO_ROOT,          {.i = MOVE_WINDOW}      },
    { XCB_BUTTON_INDEX_3,  MOD,           RUN         ,    ROOT_ONLY,        {.com = context_menu}   },
    { XCB_BUTTON_INDEX_3,  MOD,           MOUSE_MOTION,    NO_ROOT,          {.i = RESIZE_WINDOW}    },
};

#endif

#ifndef NO_KEYS

#include <X11/keysymdef.h>
#include <X11/keysym.h>

/*user changes here*/
static struct key keys[] = {
    /* { key, modifier, function, window modifier, param [ config / command ]  */
    //open a terminal
    { XK_Return,           MOD,           RUN,             NONE,             {.com = terminal}       },
    //open rofi
    { XK_space,            MOD,           RUN,             NONE,             {.com = fixed_menu}     },
    //reload polybar
    { XK_r,                MOD|CONTROL,   RUN,             NONE,             {.com = reload_polybar} },
    //close window
    { XK_q,                MOD,           CLOSE_WINDOW,    NO_ROOT,          {.i = NONE}             },
    //reload wm
    { XK_r,                MOD|SHIFT,     RELOAD_CONFIG,   NONE,             {}                      },
    //kill window
    { XK_q,                MOD|SHIFT,     CLOSE_WINDOW,    NO_ROOT,          {.i = KILL}             },

    //center window
    { XK_c,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = NONE}           },
    //center window on x axis
    { XK_x,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = X}              },
    //center window on y axis
    { XK_y,                MOD,           CENTER_WINDOW,   NO_ROOT,          {.i = Y}              },

    {XK_Print,             NONE,          RUN,             NONE,             {.com = print}        },

    {XK_b,                 MOD,           TOGGLE_BORDER,   NO_ROOT,          {}                    },
    {XK_f,                 MOD,           TOGGLE_MAXIMIZE, NO_ROOT,          {}                    },
    {XK_m,                 MOD,           TOGGLE_MONOCLE,  NO_ROOT,          {}                    },

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


#pragma once

#include "utils.h"

#include "include/types.h"
#include "include/handlers.h"
#include "include/events.h"

#define command(com_name, com, ...) \
    __attribute_used__ \
    static char * com_name[] = { com, ##__VA_ARGS__, 0 };

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

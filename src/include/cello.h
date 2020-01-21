#pragma once

#include "utils.h"

#include "window.h"
#include "desktop.h"

#include <xcb/xcb.h>

#define WMNAME "Cello"
#define WMNAMELEN 5

#define WMSOCKPATH "/tmp/cellowm-%s-%d.%d.sock"


#define CONFIG_PATH "/.config/cellowm/config.json"
#define CONFIG_PATH_LEN 28

#define CELLO_STATE_NORMAL   ( 1 << 0 )
#define CELLO_STATE_MAXIMIZE ( 1 << 1 )
#define CELLO_STATE_FOCUS    ( 1 << 2 )
#define CELLO_STATE_BORDER   ( 1 << 3 )

/*global connection*/
extern xcb_connection_t * conn;
/*global root screen*/
extern xcb_screen_t * root_screen;
extern struct window_list * dslist[MAX_DESKTOPS];

extern struct config conf;

extern bool run;

xcb_atom_t WM_DELETE_WINDOW;

xcb_screen_t * get_screen(xcb_connection_t con, int scr);

struct window * window_configure_new(xcb_window_t win);

void cello_add_window_to_desktop(struct window * w, uint32_t ds);
void cello_add_window_to_current_desktop(struct window * w);
uint32_t cello_get_current_desktop();

void cello_unmap_win_from_desktop(struct window * w);
void cello_unmap_window(struct window * w);
void cello_destroy_window(struct window * w);

void window_maximize(struct window * w, uint16_t stt);
void cello_unmaximize_window(struct window * w);
void cello_goto_desktop(uint32_t d);

void cello_update_wilist_with(xcb_drawable_t wid);
void cello_update_wilist();

void cello_reload();
void cello_setup_all();

void cello_exit();
void cello_deploy();

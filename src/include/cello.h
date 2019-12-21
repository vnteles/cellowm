#pragma once

#include "utils.h"

#include <xcb/xcb.h>

#define WMNAME "Cello"
#define WMNAMELEN 5

#define CONFIG_PATH "/.config/cellowm/config.json"
#define CONFIG_PATH_LEN 28

#define MAX_DESKTOPS 10

#define DECO_NONE (1<<0)
#define DECO_BORDER (1<<1)
#define DECO_NO_BORDER (1<<2)

/*global connection*/
extern xcb_connection_t * conn;
/*global root screen*/
extern xcb_screen_t * root_screen;

extern struct config conf;

xcb_atom_t WM_DELETE_WINDOW;


xcb_screen_t * get_screen(xcb_connection_t con, int scr);

struct window * cello_find_window(xcb_drawable_t wid);
struct window * cello_configure_new_window(xcb_window_t win);

void cello_add_window_to_desktop(struct window * w, uint32_t ds);
void cello_add_window_to_current_desktop(struct window * w);
uint32_t cello_get_current_desktop();

void cello_decorate_window(struct window * w);

void cello_unmap_win_from_desktop(struct window * w);
void cello_unmap_window(struct window * w);
void cello_destroy_window(xcb_window_t win);

void cello_move_window(xcb_window_t w, int16_t x, int16_t y);

void cello_center_window(struct window * w);
void cello_center_window_x(struct window * w);
void cello_center_window_y(struct window * w);

void cello_goto_desktop(uint32_t d);

void cello_update_wilist_with(xcb_drawable_t wid);
void cello_update_wilist();

void cello_reload();
bool cello_setup_all();

void cello_exit();
void cello_deploy();

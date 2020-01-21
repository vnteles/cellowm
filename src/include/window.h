#pragma once

#include "types.h"

/**
 ** the global window list
 **/
extern struct window_list * wilist;

/*get the window from node*/
#define __Node2Window__(n, w) w = (struct window *) n->window

/**
 ** @brief find a window from the client list by passing an id
 ** @param wid the id of the window you're lookin for
 ** @return the window if id matches, or null if the window is not handled by the wm nor doesn't exists
 **/
struct window *find_window_by_id(xcb_drawable_t wid);

/**
 ** @brief move the window to the center of the screen
 ** @param w the window to be centered
 **/
void center_window(struct window * w);

/**
 ** @brief move the window to the center of the screen on x axis
 ** @param w the window to be centered
 **/
void center_window_x(struct window * w);

/**
 ** @brief move the window to the center of the screen on y axis
 ** @param w the window to be centered
 **/
void center_window_y(struct window * w);

/**
 ** @brief handle all windows already mapped when the connection was created
 **/
void window_hijack();

/**
 ** @brief get the next or the previous window from the focused one
 ** @param prev if true, get the previous window, the next if false
 ** @return a window
 **/
struct window * nextprev_window(bool prev);

#define prev_window() nextprev_window(true)
#define next_window() nextprev_window(false)

/**
 ** @brief update the window decoration
 **/ 
void update_decoration(struct window * w);


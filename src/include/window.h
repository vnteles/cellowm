#pragma once

#include "types.h"

extern struct list * wilist;

/*get the window from node*/
#define __Node2Window__(n, w) w = (struct window *) n->gdata

/*move the window to the center of the screen*/
void window_center(struct window * w);
/*move the window to the center of the screen on x axis*/
void window_center_x(struct window * w);
/*move the window to the center of the screen on y axis*/
void window_center_y(struct window * w);

/*handle all windows already mapped when the connection was created*/
void window_hijack();

/*update the window decoration*/ 
void window_decorate(struct window * w);


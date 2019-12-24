#pragma once

#include <xcb/xcb.h>

#include "types.h"


void MOUSE_MOTION(const union param * param);
void RUN(const union param * param);
void CLOSE_WINDOW(const union param * param);
void CENTER_WINDOW(const union param * param);
void RELOAD_CONFIG(const union param * param);
void TOGGLE_BORDER(const union param * param);
void TOGGLE_MAXIMIZE(const union param * param);
void TOGGLE_MONOCLE(const union param * param);
void CHANGE_DESKTOP(const union param * param);
void MOVE_WINDOW_TO_DESKTOP(const union param * param);

void handle(xcb_generic_event_t * e);
void handle_events();
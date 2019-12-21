#pragma once

#include <xcb/xcb.h>

#include "types.h"

void MOUSE_MOTION(const union param * param);
void RUN(const union param * param);
void CLOSE_WINDOW(const union param * param);
void CENTER_WINDOW(const union param * param);
void RELOAD_CONFIG(const union param * param);
void TOGGLE_BORDER(const union param * param);
void CHANGE_DESKTOP(const union param * param);
void MOVE_WINDOW_TO_DESKTOP(const union param * param);

void handle_events();
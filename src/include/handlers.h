#pragma once

#include <xcb/xcb.h>

#include "types.h"

/**
 ** @brief handle the given event based on the wm event list
 ** @param e the event to be handled
 **/
void handle_event(xcb_generic_event_t * e);
void handle_message(char * msg, int msg_len, int fd);

void RUN(const union param * param);
void MOUSE_MOTION(const union param * param);
void CLOSE_WINDOW(const union param * param);
void CENTER_WINDOW(const union param * param);
void RELOAD_CONFIG(const union param * param);
void TOGGLE_BORDER(const union param * param);
void TOGGLE_MAXIMIZE(const union param * param);
void TOGGLE_MONOCLE(const union param * param);
void CHANGE_DESKTOP(const union param * param);
void MOVE_WINDOW_TO_DESKTOP(const union param * param);

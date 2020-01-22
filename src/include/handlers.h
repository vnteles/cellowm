#pragma once

#include <xcb/xcb.h>

#include "types.h"

/**
 ** @brief handle the given event based on the wm event list
 ** @param e the event to be handled
 **/
void handle_event(xcb_generic_event_t * e);
void handle_message(char * msg, int msg_len, int fd);

void RUN(const union action param);
void MOUSE_MOTION(const union action param);
void CLOSE_WINDOW(const union action param);
void CENTER_WINDOW(const union action param);
void RELOAD_CONFIG(const union action param);
void TOGGLE_BORDER(const union action param);
void TOGGLE_MAXIMIZE(const union action param);
void TOGGLE_FOCUS_MODE(const union action param);
void CHANGE_DESKTOP(const union action param);
void MOVE_WINDOW_TO_DESKTOP(const union action param);

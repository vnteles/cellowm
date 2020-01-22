#pragma once

#include "cello.h"

extern void (*events[0x7F])(xcb_generic_event_t *);

void on_mouse_motion(int8_t action);

void init_events();
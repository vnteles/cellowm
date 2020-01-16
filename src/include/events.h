#pragma once

#include "cello.h"

extern void (*events[0x7F])(xcb_generic_event_t *);

void init_events();
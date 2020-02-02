#pragma once

#define MAX_DESKTOPS 10

extern struct window_list * dslist[MAX_DESKTOPS];

void unmap_desktop(uint32_t ds);
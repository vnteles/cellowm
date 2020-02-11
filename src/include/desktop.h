#pragma once

#define MAX_DESKTOPS 10

#define DEFAULT_DESKTOP_NUMBER 6

__attribute__((used))
static char * DEFAULT_DESKTOP_NAMES[] = {
    "one", "two", "three",
    "four", "five", "six"
};

extern struct window_list * dslist[MAX_DESKTOPS];

void unmap_desktop(uint32_t ds);
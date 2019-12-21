#pragma once

#include <stdio.h>

#define _inner_border 0
#define _inner_border_color 1
#define _outer_border 2
#define _outer_border_color 3
#define _border 4
#define _keys 5
#define _buttons 6

#define uknw -1

void parse_json_config(void * vconfig_file);
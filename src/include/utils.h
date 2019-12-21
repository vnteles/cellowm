#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "types.h"

#define NONE 0x0

#define RESIZE_WINDOW 1<<0
#define MOVE_WINDOW 1<<1

#define ROOT_ONLY 1<<0
#define NO_ROOT 1<<1

#define KILL 1<<0

#define X 1<<0
#define Y 1<<1

/* switch m1 to m2 from mask */
#define __SWITCH_MASK__(mask, m1, m2) mask &= ~m1; mask |= m2

/* Generally we don't need to allocate too much space,
 * thats why I'm using uint16 instead of size_t
 */
void * umalloc(uint16_t size);
void * ucalloc(uint16_t qnt, uint16_t size);
void * urealloc(void * ptr, uint16_t size);

char * strndup(const char *s, size_t n);

long getncolor(const char *hex, int n);
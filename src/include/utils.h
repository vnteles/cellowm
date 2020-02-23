#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "desktop.h"

#define NONE 0x0

#define RESIZE_WINDOW ( 1 << 0 )
#define MOVE_WINDOW ( 1 << 1 )

#define ROOT_ONLY ( 1 << 0 )
#define NO_ROOT ( 1 << 1 )

#define KILL ( 1 << 0 )

#define X ( 1 << 0 )
#define Y ( 1 << 1 )

// get the higher value from the set (a,b)
#define max(a, b) (a > b ? a : b)

/*add the mask `m` to `maskv`*/
#define __AddMask__(maskv, m) maskv |= m
/*remove the mask `m` from `maskv`*/
#define __RemoveMask__(maskv, m) maskv &= ~m
/*remove `m1` and add `m2` to `maskv`*/
#define __SwitchMask__(maskv, m1, m2) __RemoveMask__(maskv, m1); __AddMask__(maskv, m2)

/* compare two strings. Return true if the strings match */
#define comp_str(s1, s2) (strcmp(s1, s2) == 0)

void * umalloc(size_t size);
void * ucalloc(uint32_t qnt, size_t size);

#define ufree(ptr) { free(ptr); ptr = NULL; }

char * strndup(const char *s, size_t n);
long getncolor(const char *hex, int n);
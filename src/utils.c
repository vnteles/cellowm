#include "utils.h"

#include <unistd.h>
#include <string.h>

#include "log.h"


void * umalloc(uint16_t size);
void * ucalloc(uint16_t qnt, uint16_t size);
void * urealloc(void * ptr, uint16_t size);

#define mem_error() \
    CRITICAL("{!} Unexpected Memory error\n");

void * umalloc(uint16_t size) {
    void * ptr;
    if ( (ptr = malloc(size)) )  return ptr;
    else mem_error();

    /*unreachable*/
    return NULL;
}

void * ucalloc(uint16_t qnt, uint16_t size) {
    void * ptr;
    if ( (ptr = calloc(qnt, size)) )  return ptr;
    else mem_error();

    return NULL;
}

void * urealloc(void * ptr, uint16_t size) {
    if ( (ptr = realloc(ptr, size)) )  return ptr;
    else mem_error();

    return NULL;
}

char * strndup(const char *s, size_t n) {
    char *p = memchr(s, '\0', n);
    if (p != NULL)
        n = p - s;
    p = malloc(n + 1);
    if (p != NULL) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

long getncolor(const char * hex, int n) {
    /* assert to be a solid color */
	long ulhex = 0xff000000;
    /* base color lenght, without `#` (only rgb or rrggbb) */
    uint8_t lhex = n-1;

    /* invalid color */
    if (hex[0] != '#' || (lhex != 3 && lhex != 6)) return 0;

    char hex_group[7];
    hex++;

    /* handle colors in #rgb format */
    if (lhex == 3){
        /*r*/ hex_group[0] = hex_group[1] = hex[0];
        /*g*/ hex_group[2] = hex_group[3] = hex[1];
        /*b*/ hex_group[4] = hex_group[5] = hex[2];
        hex_group[6] = 0; 
    } else strncpy(hex_group, hex, 6); /* handle colors in #rrggbb format */

    //todo: check if the conversion was successful
    return (ulhex | strtol(hex_group, NULL, 16));
}
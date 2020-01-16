#pragma once

#include <stdio.h>
#include <unistd.h>
#include "utils.h"

#include <time.h>

__attribute__((used))
static void logtime(char *s) {
    time_t t = time ( NULL );
    struct tm * tm = localtime ( &t );
    sprintf(s, "[%d/%d/%d %d:%d:%d]", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

#ifndef NO_DEBUG
    /* Normal log */
    #define NLOG(log, ...) \
        { fprintf(stdout, "{@} "log"\n", ##__VA_ARGS__); fflush(stdout); }

    /*----------------debug stuff------------------*/
    /* Debug log */
    #define DLOG(log, ...) \
        { char s[30]; logtime(s); fprintf(stdout, "%s "log"\n", s, ##__VA_ARGS__); fflush(stdout); }


    // enter function
    #define EFN(f) \
        { fprintf(stdout, "\n_____ %s : %p _____\n", #f, f); }

    #define VVAL(X) fprintf(stdout, _Generic((X),  \
        char: "%c",                 \
        uint8_t: "%d",              \
        uint16_t: "%d",             \
        uint32_t: "%d",             \
        bool: "%d",                 \
        int: "%d",                  \
        char *: "%s",               \
        default: "%p"               \
    ), X);

    #define VARDUMP(v) { \
        fprintf(stdout, "vardump: " #v " = "); \
        VVAL(v);                        \
        fprintf(stdout, "\n");          \
        fflush(stdout);                 \
    }

#else
    #define NLOG(n, ...);
#endif

/* Error log */
#define ELOG(err, ...) \
    { fprintf(stderr, "{#} "err"\n", ##__VA_ARGS__); fflush(stdout); }
/* Critical log */
#define CRITICAL(err, ...) {                            \
        fprintf(stderr, "{!!} "err"\n", ##__VA_ARGS__); \
        fflush(stdout);                                 \
        exit(EXIT_FAILURE);                             \
    }

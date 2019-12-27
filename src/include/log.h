#pragma once

#include <stdio.h>
#include <unistd.h>
#include "utils.h"

#ifndef NO_DEBUG
    /* Error log */
    #define ELOG(err, ...) \
        { (void) fprintf(stderr, err, ##__VA_ARGS__); }

    /* Normal log */
    #define NLOG(log, ...) \
        { (void) fprintf(stdout, log, ##__VA_ARGS__); }

#else

    #define ELOG(n, ...);
    #define NLOG(n, ...);

#endif

/* Critical log */
#define CRITICAL(err, ...) \
    { (void) fprintf(stderr, err, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); }

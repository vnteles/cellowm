#include <errno.h>
#include <signal.h>
#include <string.h>

#include "cello.h"

extern int errno;

char * path;

int main(__attribute__((unused)) int argc, char ** argv) {

    /* grab signals to ensure the garbage collection */
    signal(SIGINT,  &exit);
    signal(SIGKILL, &exit);
    signal(SIGTERM, &exit);

    path = strndup(argv[0], strlen(argv[0]));

    /* ensure the wm setup before we can execute */
    cello_setup_all();
    cello_deploy();

    return errno;
}
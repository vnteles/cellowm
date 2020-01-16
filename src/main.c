#include <signal.h>

#include "cello.h"

/* as we are not handling arguments yet, we don't need to load the args without the dev reload */

int main() {

    /* grab signals to ensure the garbage collection */
    signal(SIGINT,  &exit);
    signal(SIGKILL, &exit);
    signal(SIGTERM, &exit);

    /* ensure the wm setup before we can execute */
    if (cello_setup_all())
        cello_deploy();
    else 
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
#include <errno.h>
#include <signal.h>


#include "cello.h"


extern int errno;

int main() {

    /* grab signals to ensure the garbage collection */
    signal(SIGINT,  &exit);
    signal(SIGKILL, &exit);
    signal(SIGTERM, &exit);

    /* ensure the wm setup before we can execute */
    cello_setup_all();
    cello_deploy();

    return errno;
}
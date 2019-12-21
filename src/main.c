#include <signal.h>

#include "cello.h"

/* as we are not handling arguments yet, we don't need to load the args without the dev reload */
#ifdef DEVRELOAD
char * execpath = NULL;
/* not using argc yet */
int main( int argc __attribute((unused)), char * argv[] ) {
    /* exec path to dev reload */
    execpath = argv[0];
#else
int main() {
#endif
    /* grab signals to ensure the garbage collection */
    signal(SIGINT,  &exit);
    signal(SIGKILL, &exit);
    signal(SIGTERM, &exit);

    /* ensure the wm setup before we can execute */
    if (cello_setup_all()) cello_deploy();
    else return 1;

    return 0;
}
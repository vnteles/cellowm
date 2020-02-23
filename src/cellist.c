#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "cello.h"
#include "log.h"

int main(int argc, char * argv[]) {
    int sockfd;
    struct sockaddr_un addr;
    char sendbuff[BUFSIZ], recvbuff[BUFSIZ];

    int send_len = 0;//, recv_len;

    // verify the number of arguments
    if (argc < 2) CRITICAL("No Arguments passed.");
    // use AF_UNIX
    addr.sun_family = AF_UNIX;

    // start the socket
    if ((sockfd = socket(addr.sun_family, SOCK_STREAM, 0)) == -1)
        CRITICAL("Unable to start the socket");

    // set the sun_path
    {
        char * host = NULL;
        int dsp, scr;

        if (xcb_parse_display(NULL, &host, &dsp, &scr) != 0)
            snprintf(addr.sun_path, sizeof addr.sun_path, WMSOCKPATH, host, dsp, scr);

        if (host)
            ufree(host);
        
        if (connect(sockfd, (struct sockaddr *) &addr, sizeof addr) == -1)
            CRITICAL("Unable to connect to the socket");

    }
    int max = sizeof sendbuff;
    for (int i = 1; max > 0 && i < argc; i++) {
        send_len += snprintf(sendbuff+send_len, max, "%s%c", argv[i], 0);
        max-=send_len;
    }

    if (send(sockfd, sendbuff, send_len, 0) < send_len) {
        CRITICAL("Could not send the message to the socket");
    }
    // DLOG("Message sent");

    close(sockfd);
    return EXIT_SUCCESS;
}
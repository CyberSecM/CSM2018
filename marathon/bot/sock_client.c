#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define SOCK_ERR         -1
#define SOCK_ERRSTR_LEN 512

#define handle_cmd(x) if(!strcmp(x, cmd))
#define sperror(s) fprintf(stderr, s);
struct addrinfo *prepare_addrinfo_tcp(const char *addr, const char *port, char *errorstr) {
    struct addrinfo hints, *res;
    int gai;

    /*  init addrinfo */
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;     // fill in my IP for me

    if (( gai = getaddrinfo(addr, port, &hints, &res)) != 0) {
        if (errorstr != NULL)
            sperror("Error\n");
        return NULL;
    }

    return res;
}

int tcp_client(char const *server_addr, char const *port, char *errorstr)
{
    struct addrinfo *servinfo, *p;
    int              sockfd;
    if ( (servinfo = prepare_addrinfo_tcp(server_addr, port, errorstr)) == NULL)
        return SOCK_ERR;

    /* loop through all the results and connect to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {

        /* socket creation */
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) < 0) {
            sperror("socket");
            continue;
        }

        /* connect */
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            //sperror("connect");
            continue;
        }

        break;
    }

    if (p == NULL)
        sockfd = SOCK_ERR;

    freeaddrinfo(servinfo); // all done with this structure
    return sockfd;
}

int tcp_recvline(int fd, char* res, int len) {
    int i = 0;
    while(1) {
        recv(fd, &res[i], 1, 0);
        if(res[i] == '\n' || i >= len)
            return i;
    }
    return i;
}

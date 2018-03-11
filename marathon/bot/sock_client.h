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
void STRERROR(int errnum, char *errorstr);

#define CONST_STRLEN(s) (sizeof(s)/sizeof(char)-1)

#define sperror(where) do {                                     \
    if (errorstr != NULL) {                                     \
        memcpy(errorstr, where ": ", CONST_STRLEN(where ": ")); \
        STRERROR(errno, errorstr+CONST_STRLEN(where ": "));     \
    }                                                           \
} while (0)
struct addrinfo *prepare_addrinfo_tcp(const char *addr, const char *port, char *errorstr);
int tcp_client(char const *server_addr, char const *port, char *errorstr);
int tcp_recvline(int fd, char* res, int len);

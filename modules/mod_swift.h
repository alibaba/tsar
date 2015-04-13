#ifndef MOD_SWIFT_H
#define MOD_SWIFT_H

#define EQUAL ":="
#define DEBUG 1

typedef int (parse_swift_info_func)(char *buf);

inline int my_swift_net_connect(const char *host_name, int port, int *sd, char* proto)
{
    int                 result;
    struct protoent    *ptrp;
    struct sockaddr_in  servaddr;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp=getprotobyname(proto)))==NULL) {
        if (DEBUG) {
            printf("Cannot map \"%s\" to protocol number\n", proto);
        }
        return 3;
    }

    /* create a socket */
    *sd = socket(PF_INET, (!strcmp(proto, "udp"))?SOCK_DGRAM:SOCK_STREAM, ptrp->p_proto);
    if (*sd < 0) {
        close(*sd);
        if (DEBUG) {
            printf("Socket creation failed\n");
        }
        return 3;
    }

    /* open a connection */
    result = connect(*sd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (result < 0) {
        close(*sd);
        switch (errno) {
            case ECONNREFUSED:
                if (DEBUG) {
                    printf("Connection refused by host\n");
                }
                break;
            case ETIMEDOUT:
                if (DEBUG) {
                    printf("Timeout while attempting connection\n");
                }
                break;
            case ENETUNREACH:
                if (DEBUG) {
                    printf("Network is unreachable\n");
                }
                break;
            default:
                if (DEBUG) {
                    printf("Connection refused or timed out\n");
                }
        }

        return 2;
    }
    return 0;
}

inline ssize_t mywrite_swift(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

inline ssize_t myread_swift(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

inline int read_swift_value(char *buf, const char *key, unsigned long long *ret)
{
    int    k = 0;
    char  *tmp;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {
        /* compute the offset */
        k = strcspn(tmp, EQUAL);
        sscanf(tmp + k + 1, "%lld", ret);
        return 1;

    } else {
        return 0;
    }
}

#endif

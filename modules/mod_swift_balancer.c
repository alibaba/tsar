#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"

#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL "="
#define DEBUG 1

char *swift_balancer_usage = "    --swift_balancer    Swift L7 balancer infomation";
int mgrport = 82;

/* string at swiftclient -p 82 mgr:info */
/*
 * balancer_http.requests = 67541388
 * balancer_http.total_svc_time = 320478065
 * balancer_http.hits = 53265552
 * balancer_http.bytes_in = 22783683510
 * balancer_http.bytes_out = 109863216623
 */
const static char *SWIFT_BALANCER[] = {
    "balancer_http.requests",
    "balancer_http.total_svc_time",
    "balancer_http.hits",
    "balancer_http.bytes_in",
    "balancer_http.bytes_out"
};

/* struct for swift counters */
struct status_swift_balancer {
    unsigned long long requests;
    unsigned long long rt;
    unsigned long long r_hit;
    unsigned long long bytes_in;
    unsigned long long bytes_out;
} stats;

/* swift register info for tsar */
struct mod_info swift_balancer_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", DETAIL_BIT,  0,  STATS_NULL},
    {" r_hit", DETAIL_BIT,  0,  STATS_NULL},
    {" in_bw", DETAIL_BIT,  0,  STATS_NULL},
    {"out_bw", DETAIL_BIT,  0,  STATS_NULL}
};
/* opens a tcp or udp connection to a remote host or local socket */
int
my_swift_balancer_net_connect(const char *host_name, int port, int *sd, char *proto)
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

ssize_t
mywrite_swift_balancer(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread_swift_balancer(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
int
read_swift_balancer_value(char *buf, const char *key, unsigned long long *ret)
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

int
parse_swift_balancer_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_balancer_value(line, SWIFT_BALANCER[0], &stats.requests);
        read_swift_balancer_value(line, SWIFT_BALANCER[1], &stats.rt);
        read_swift_balancer_value(line, SWIFT_BALANCER[2], &stats.r_hit);
        read_swift_balancer_value(line, SWIFT_BALANCER[3], &stats.bytes_in);
        read_swift_balancer_value(line, SWIFT_BALANCER[4], &stats.bytes_out);
        line = strtok(NULL, "\n");
    }
    return 0;
}

void
set_swift_balancer_record(struct module *mod, double st_array[],
                          U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] -  pre_array[0]) * 1.0 / inter;
    } else {
        st_array[0] = 0;
    }
    /* rt */
    if (st_array[0] > 0 && cur_array[1] >= pre_array[1])
        st_array[1] = (cur_array[1] - pre_array[1]) * 1.0 / st_array[0] / inter;
    else
        st_array[1] = 0;
    /* r_hit */
    if (st_array[0] > 0 && cur_array[2] >= pre_array[2])
        st_array[2] = (cur_array[2] - pre_array[2]) * 100.0 / st_array[0] / inter;
    else
        st_array[2] = 0;

    /* in_bw */
    if (cur_array[3] >= pre_array[3]) {
        st_array[3] = (cur_array[3] -  pre_array[3]) / inter;
    } else {
        st_array[3] = 0;
    }

    /* out_bw */
    if (cur_array[4] >= pre_array[4]) {
        st_array[4] = (cur_array[4] -  pre_array[4]) / inter;

    } else {
        st_array[4] = 0;
    }
}

int
read_swift_balancer_stat(char *cmd)
{
    char msg[LEN_512];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/%s "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n",
            cmd);

    int len, conn, bytesWritten, fsize = 0;

    if (my_swift_balancer_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    int flags;

    /* set socket fd noblock */
    if ((flags = fcntl(conn, F_GETFL, 0)) < 0) {
        close(conn);
        return -1;
    }

    if (fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(conn);
        return -1;
    }

    struct timeval timeout = {10, 0};

    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

    bytesWritten = mywrite_swift_balancer(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_balancer(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_swift_balancer_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

void
read_swift_balancer_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096];

    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 82;
    }
    while (read_swift_balancer_stat("counters") < 0 && retry < RETRY_NUM) {
        retry++;
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld",
                  stats.requests,
                  stats.rt,
                  stats.r_hit,
                  stats.bytes_in,
                  stats.bytes_out);
    buf[pos] = '\0';
    //fprintf(stderr, "buf: %s\n", buf);
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_balancer", swift_balancer_usage, swift_balancer_info, 5, read_swift_balancer_stats, set_swift_balancer_record);
}

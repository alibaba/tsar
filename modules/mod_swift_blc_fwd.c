#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"

#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL "="
#define DEBUG 0

char  *swift_blc_fwd_usage = "    --swift_blc_fwd     Swift forward to Balancer infomation";
int    mgrport = 82;

/* swiftclient -p 82 mgr:counters */
/*
   blc_fwd_http.requests = 13342113
   blc_fwd_http.errors = 220
   blc_fwd_http.bytes_in = 210982517709
   blc_fwd_http.bytes_out = 0
   blc_fwd_http.svc_time = 1526450363
 */
const static char *SWIFT_BLC_FWD[] = {
    "blc_fwd_http.requests",
    "blc_fwd_http.errors",
    "blc_fwd_http.bytes_in",
    "blc_fwd_http.svc_time"
};

/* struct for httpfwd counters */
struct status_swift_blc_fwd {
    unsigned long long requests;
    unsigned long long errors;
    unsigned long long bytes_in;
    unsigned long long svc_time;
} stats;

/* swift register info for tsar */
struct mod_info swift_blc_fwd_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {" traff", DETAIL_BIT,  0,  STATS_NULL},
    {" error", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", DETAIL_BIT,  0,  STATS_NULL}
};
/* opens a tcp or udp connection to a remote host or local socket */
int
my_swift_blc_fwd_net_connect(const char *host_name, int port, int *sd, char *proto)
{
    int    result;
    struct sockaddr_in servaddr;
    struct protoent *ptrp;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp = getprotobyname(proto)))==NULL) {
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
mywrite_swift_blc_fwd(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread_swift_blc_fwd(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
int
read_swift_blc_fwd_value(char *buf,
                         const char *key,
                         unsigned long long *ret)
{
    int    k=0;
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
parse_swift_blc_fwd_info(char *buf)
{
    char   *line;
    line = strtok(buf, "\n");
    while(line != NULL) {
        read_swift_blc_fwd_value(line, SWIFT_BLC_FWD[0], &stats.requests);
        read_swift_blc_fwd_value(line, SWIFT_BLC_FWD[1], &stats.errors);
        read_swift_blc_fwd_value(line, SWIFT_BLC_FWD[2], &stats.bytes_in);
        read_swift_blc_fwd_value(line, SWIFT_BLC_FWD[3], &stats.svc_time);
        line = strtok(NULL, "\n");
    }
    return 0;
}

void
set_swift_blc_fwd_record(struct module *mod, double st_array[],
                         U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col - 1; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;

        } else {
            st_array[i] = -1;
        }
    }
    if(cur_array[i] >= pre_array[i] && st_array[0] > 0) {
        st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / st_array[0] / inter;

    } else {
        st_array[i] = -1;
    }
}

int
read_swift_blc_fwd_stat()
{
    int    len, conn, bytesWritten, fsize = 0;
    char   msg[LEN_512];
    char   buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/counters "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    if (my_swift_blc_fwd_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    int    flags;

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

    bytesWritten = mywrite_swift_blc_fwd(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_blc_fwd(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_swift_blc_fwd_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

void
read_swift_blc_fwd_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096];
    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 82;
    }
    while (read_swift_blc_fwd_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld",
                  stats.requests,
                  stats.bytes_in,
                  stats.errors,
                  stats.svc_time
                 );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_blc_fwd", swift_blc_fwd_usage, swift_blc_fwd_info, 4, read_swift_blc_fwd_stats, set_swift_blc_fwd_record);
}

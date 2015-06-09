
/*author:haoyun.lt@alibaba-inc.com*/

#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL ":="
#define DEBUG 1

static char *swift_usage = "    --swift_swapdir     Swift disk info";
static int mgrport = 82;

#define MAX_PARTITIONS 128

/* string at swiftclient -p 81 mgr:swapdir */
/*
 * Store Directory #0 (tcoss): /dev/sda6
 * Maximum Size: 81920 KB
 * start offset: 16777216
 * end offset  : 100663296
 * Current Size: 73735 KB
 * Percent Used: 90.01%
 * Current load metric: 1 / 1000
 * Object Count: 1793
 * curstripe: 9
 */

static struct part_info {
    char name[128];
    char type[128];
} partition[MAX_PARTITIONS];

/* struct for swift counters */
static struct status_swift_swapdir {
    unsigned long long current_size;
    unsigned long long object_count;
    unsigned long long used;
    unsigned long long start;
    unsigned long long end;
    unsigned long long current_stripe;
    unsigned long long offset;
} swapdir_stats[MAX_PARTITIONS];

/* swift register info for tsar */
struct mod_info swift_swapdir_info[] = {
    {"  size", DETAIL_BIT,  0,  STATS_NULL},
    {" count", DETAIL_BIT,  0,  STATS_NULL},
    {"  used", DETAIL_BIT,  0,  STATS_NULL},
    {"stripe", DETAIL_BIT,  0,  STATS_NULL},
    {"offset", DETAIL_BIT,  0,  STATS_NULL},
    {"  cost", DETAIL_BIT,  0,  STATS_NULL},
    {"  null", HIDE_BIT,  0,  STATS_NULL}
};
/* opens a tcp or udp connection to a remote host or local socket */
static int
my_swift_net_connect(const char *host_name, int port, int *sd, char *proto)
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

static ssize_t
mywrite_swift(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

static ssize_t
myread_swift(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

static int
parse_swift_swapdir_info(char *buf)
{
    char *line;
    int n_swapdir = -1;
    line = strtok(buf, "\n");
    while (line != NULL) {
        if (n_swapdir >= MAX_PARTITIONS - 1)
            return MAX_PARTITIONS;
        if (strstr(line, "Store Directory") != NULL) {
            /* Store Directory #0 (tcoss): /dev/sda6 */
            int fileno;
            char type[128] = {0}, path[128] = {0};
            sscanf(line, "Store Directory #%d (%[^)]): %s", &fileno, (char *)&type, (char *)&path);
            n_swapdir += 1;
            snprintf(partition[n_swapdir].name, 128, "%s", path);
            snprintf(partition[n_swapdir].type, 128, "%s", type);
        } else if (n_swapdir < 0){
            return n_swapdir + 1;
        } else if (strstr(line, "start offset") != NULL) {
            /* start offset: 16777216 */
            unsigned long long size = 0;
            sscanf(line, "start offset: %lld", &size);
            swapdir_stats[n_swapdir].start = size;
        } else if (strstr(line, "end offset") != NULL) {
            /* end offset  : 100663296 */
            unsigned long long size = 0;
            sscanf(line, "end offset  : %lld", &size);
            swapdir_stats[n_swapdir].end = size;
        } else if (strstr(line, "Current Size") != NULL) {
            /* Current Size: 73735 KB */
            unsigned long long size = 0;
            sscanf(line, "Current Size: %lld KB", &size);
            swapdir_stats[n_swapdir].current_size = size << 10;
        } else if (strstr(line, "Object Count") != NULL) {
            /* Object Count: 1793 */
            unsigned long long count = 0;
            sscanf(line, "Object Count: %lld", &count);
            swapdir_stats[n_swapdir].object_count = count;
        } else if (strstr(line, "Percent Used") != NULL) {
            /* Percent Used: 90.01% */
            float per = 0;
            sscanf(line, "Percent Used: %f%%", &per);
            swapdir_stats[n_swapdir].used = per;
        } else if (strstr(line, "curstripe") != NULL) {
            /* curstripe: 9 */
            unsigned long long stripe = 0;
            sscanf(line, "curstripe: %lld", &stripe);
            swapdir_stats[n_swapdir].current_stripe = stripe;
        }

        line = strtok(NULL, "\n");
    }
    n_swapdir += 1;
    return n_swapdir;
}

void
set_swift_swapdir_record(struct module *mod, double st_array[],
                         U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[0];
    st_array[1] = cur_array[1];
    st_array[2] = cur_array[2];
    st_array[3] = cur_array[3];
    st_array[4] = cur_array[4];
    if (cur_array[5] - pre_array[5] <= 0) {
        st_array[5] = -1;
    } else {
        st_array[5] = 100 * inter / (cur_array[5] - pre_array[5]);
    }
}

static int
read_swift_swapdir_stat(char *cmd)
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

    int len, conn = -1, bytesWritten, fsize = 0, n;

    if (my_swift_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        if (conn != -1)
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

    bytesWritten = mywrite_swift(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift(conn, buf + fsize, sizeof(buf) - fsize - 1)) > 0) {
        fsize += len;
    }
    buf[fsize] = '\0';

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    n = parse_swift_swapdir_info(buf);
    if (n <= 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return n;
}

void
read_swift_swapdir_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0, p = 0, n_swapdir;
    char   buf[LEN_4096];

    for (p = 0; p < MAX_PARTITIONS; p++) {
        memset(&swapdir_stats[p], 0, sizeof(swapdir_stats[p]));
    }

    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 81;
    }
    n_swapdir = read_swift_swapdir_stat("swapdir");
    while (n_swapdir <= 0 && retry < RETRY_NUM) {
        retry++;
        n_swapdir = read_swift_swapdir_stat("swapdir");
    }

    for (p = 0; p < n_swapdir && p < MAX_PARTITIONS; p++) {
        swapdir_stats[p].offset = 100 * swapdir_stats[p].current_stripe / ((swapdir_stats[p].end - swapdir_stats[p].start) >> 23);
        pos += sprintf(buf + pos, "%s=%lld,%lld,%lld,%lld,%lld,%lld",
                       partition[p].name,
                       swapdir_stats[p].current_size,
                       swapdir_stats[p].object_count,
                       swapdir_stats[p].used,
                       swapdir_stats[p].current_stripe,
                       swapdir_stats[p].offset,
                       swapdir_stats[p].offset
                      );
        pos += sprintf(buf + pos, ITEM_SPLIT);
    }

    if (pos) {
        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--swift_swapdir", swift_usage, swift_swapdir_info, 6, read_swift_swapdir_stats, set_swift_swapdir_record);
}


/*author:haoyun.lt@alibaba-inc.com*/

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

static char *swift_usage = "    --swift_purge       Swift purge info";
static int mgrport = 82;


/* string at swiftclient -p 82 mgr:counters */
/*
 * purge.requests = 0
 * purge.hits = 0
 * purge.misses = 0
 * purge.larges = 0
 * purge.large_reads = 0
 * purge.regexes = 0
 * purge.del_regexes = 0
 * purge.rmatches = 0
 * purge.rnomatches = 0
 */

/* struct for swift counters */
static struct status_swift_purge {
    unsigned long long request;
    unsigned long long hit;
    unsigned long long miss;
    unsigned long long large;
    unsigned long long large_read;
    unsigned long long regex;
    unsigned long long del_regex;
    unsigned long long hit_regex;
    unsigned long long miss_regex;
} purge_stats;

/* swift register info for tsar */
struct mod_info swift_purge_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {"   hit", DETAIL_BIT,  0,  STATS_NULL},
    {"   mis", DETAIL_BIT,  0,  STATS_NULL},
    {" large", DETAIL_BIT,  0,  STATS_NULL},
    {"rlarge", DETAIL_BIT,  0,  STATS_NULL},
    {"   reg", DETAIL_BIT,  0,  STATS_NULL},
    {" d_reg", DETAIL_BIT,  0,  STATS_NULL},
    {" h_reg", DETAIL_BIT,  0,  STATS_NULL},
    {" m_reg", DETAIL_BIT,  0,  STATS_NULL},
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
parse_swift_purge_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        if (strstr(line, "purge.requests") != NULL) {
            sscanf(line, "purge.requests = %lld", &purge_stats.request);
        } else if (strstr(line, "purge.hits") != NULL) {
            sscanf(line, "purge.hits = %lld", &purge_stats.hit);
        } else if (strstr(line, "purge.misses") != NULL) {
            sscanf(line, "purge.misses = %lld", &purge_stats.miss);
        } else if (strstr(line, "purge.larges") != NULL) {
            sscanf(line, "purge.larges = %lld", &purge_stats.large);
        } else if (strstr(line, "purge.large_reads") != NULL) {
            sscanf(line, "purge.large_reads = %lld", &purge_stats.large_read);
        } else if (strstr(line, "purge.regexes") != NULL) {
            sscanf(line, "purge.regexes = %lld", &purge_stats.regex);
        } else if (strstr(line, "purge.del_regexes") != NULL) {
            sscanf(line, "purge.del_regexes = %lld", &purge_stats.del_regex);
        } else if (strstr(line, "purge.rmatches") != NULL) {
            sscanf(line, "purge.rmatches = %lld", &purge_stats.hit_regex);
        } else if (strstr(line, "purge.rnomatches") != NULL) {
            sscanf(line, "purge.rnomatches = %lld", &purge_stats.miss_regex);
        }

        line = strtok(NULL, "\n");
    }
    return 1;
}

void
set_swift_purge_record(struct module *mod, double st_array[],
                         U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[0] > pre_array[0]){
        st_array[0] = (cur_array[0] -  pre_array[0]) / inter;
    }else {
        st_array[0] = 0;
    }
    if (cur_array[1] > pre_array[1]){
        st_array[1] = (cur_array[1] -  pre_array[1]) / inter;
    }else {
        st_array[1] = 0;
    }
    if (cur_array[2] > pre_array[2]){
        st_array[2] = (cur_array[2] -  pre_array[2]) / inter;
    }else {
        st_array[2] = 0;
    }
    if (cur_array[3] > pre_array[3]){
        st_array[3] = (cur_array[3] -  pre_array[3]) / inter;
    }else {
        st_array[3] = 0;
    }
    if (cur_array[4] > pre_array[4]){
        st_array[4] = (cur_array[4] -  pre_array[4]) / inter;
    }else {
        st_array[4] = 0;
    }
    st_array[5] = cur_array[5];
    st_array[6] = cur_array[6];
    st_array[7] = cur_array[7];
    st_array[8] = cur_array[8];
}

static int
read_swift_purge_stat(char *cmd)
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

    n = parse_swift_purge_info(buf);
    if (n <= 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return n;
}

void
read_swift_purge_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096] = {0};

    memset(&purge_stats, 0, sizeof(purge_stats));

    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 81;
    }

    while (read_swift_purge_stat("counters") < 0 && retry < RETRY_NUM) {
        retry++;
    }

    pos += sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                   purge_stats.request,
                   purge_stats.hit,
                   purge_stats.miss,
                   purge_stats.large,
                   purge_stats.large_read,
                   purge_stats.regex,
                   purge_stats.del_regex,
                   purge_stats.hit_regex,
                   purge_stats.miss_regex
                  );

   buf[pos] = '\0';
   set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--swift_purge", swift_usage, swift_purge_info, 9, read_swift_purge_stats, set_swift_purge_record);
}

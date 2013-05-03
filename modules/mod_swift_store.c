#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 81
#define EQUAL ":"
#define DEBUG 1

char *swift_store_usage = "    --swift_store             Swift object storage infomation";
int mgrport = 81;

/* httpstore string at swiftclient -p 81 mgr:info */
/*
 * Mean Object Size:       40.35 KB
 * StoreEntries           : 20676021
 * on-memory objects      : 21227
 * on-disk objects        : 106168780
 * Request Memory Hit Ratios:      5min: 71.3%, 60min: 71.8%
 * Request Filesystem Hit Ratios(5min):    coss: 9.8%, tcoss: 13.8%
 *
 *
 */
const static char *SWIFT_STORE[] = {
    "StoreEntries",
    "on-memory objects",
    "on-disk objects",
};

/* struct for httpstore counters */
struct status_swift_store {
    unsigned long long objs;
    unsigned long long mobj;
    unsigned long long dobj;
    unsigned long long size;
    unsigned long long m_hit;
    unsigned long long coss;
    unsigned long long tcoss;
} stats;

/* swift register info for tsar */
struct mod_info swift_store_info[] = {
    {"  objs", DETAIL_BIT,  0,  STATS_NULL},
    {"  mobj", DETAIL_BIT,  0,  STATS_NULL},
    {"  dobj", DETAIL_BIT,  0,  STATS_NULL},
    {"  size", DETAIL_BIT,  0,  STATS_NULL},
    {" m_hit", DETAIL_BIT,  0,  STATS_NULL},
    {"  coss", DETAIL_BIT,  0,  STATS_NULL},
    {" tcoss", DETAIL_BIT,  0,  STATS_NULL}
};

/* opens a tcp or udp connection to a remote host or local socket */
int
my_swift_store_net_connect(const char *host_name, int port, int *sd, char* proto)
{
    int                 result;
    struct protoent    *ptrp;
    struct sockaddr_in  servaddr;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp=getprotobyname(proto))) == NULL) {
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
mywrite_swift_store(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread_swift_store(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
int
read_swift_store_value(char *buf,
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
parse_swift_store_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_store_value(line, SWIFT_STORE[0], &stats.objs);
        read_swift_store_value(line, SWIFT_STORE[1], &stats.mobj);
        read_swift_store_value(line, SWIFT_STORE[2], &stats.dobj);
        /*Mean Object Size:       40.35 KB */
        if (strstr(line, "Mean Object Size:") != NULL) {
            float a;
            sscanf(line, "        Mean Object Size:       %f KB", &a);
            stats.size = a * 1000;
        }
        /*Request Memory Hit Ratios:      5min: 71.3%, 60min: 71.8% */
        if (strstr(line, "Request Memory Hit Ratios:") != NULL) {
            float a, b;
            sscanf(line, "        Request Memory Hit Ratios:      5min: %f%%, 60min: %f%%", &a, &b);
            stats.m_hit = a * 1000;
        }
        /*Request Filesystem Hit Ratios(5min):    coss: 9.8%, tcoss: 13.8%*/
        if (strstr(line, "Request Filesystem Hit Ratios(5min):") != NULL) {
            float a, b;
            sscanf(line, "        Request Filesystem Hit Ratios(5min):    coss: %f%%, tcoss: %f%%", &a, &b);
            stats.coss= a * 1000;
            stats.tcoss = b * 1000;
        }
        line = strtok(NULL, "\n");
    }
    return 0;
}

void
set_swift_store_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        st_array[i] = cur_array[i];
        if (i >= 4 ) {
            st_array[i] /= 1000;
        }
    }
}

int
read_swift_store_stat()
{
    char msg[LEN_512];
    char buf[LEN_4096];
    sprintf(msg,
            "GET cache_object://localhost/info "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    int len, conn, bytesWritten, fsize = 0;

    if (my_swift_store_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
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

    bytesWritten = mywrite_swift_store(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_store(conn, buf, sizeof(buf))) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_swift_store_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

void
read_swift_store_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_1024];
    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 81;
    }
    while (read_swift_store_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            stats.objs,
            stats.mobj,
            stats.dobj,
            stats.size,
            stats.m_hit,
            stats.coss,
            stats.tcoss
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_store", swift_store_usage, swift_store_info, 7, read_swift_store_stats, set_swift_store_record);
}

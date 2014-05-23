#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL "="
#define DEBUG 0

char *swift_tcmalloc_usage = "    --swift_tcmalloc    Swift tcmalloc";
int mgrport = 82;

/* httpcode string at swiftclient -p 82 mgr:mem_stats */
/*
   ------------------------------------------------
MALLOC:        4916632 (    4.7 MB) Bytes in use by application
MALLOC: +      4399104 (    4.2 MB) Bytes in page heap freelist
MALLOC: +       105992 (    0.1 MB) Bytes in central cache freelist
MALLOC: +            0 (    0.0 MB) Bytes in transfer cache freelist
MALLOC: +        15456 (    0.0 MB) Bytes in thread cache freelists
MALLOC: +       655360 (    0.6 MB) Bytes in malloc metadata
MALLOC:   ------------
MALLOC: =     10092544 (    9.6 MB) Actual memory used (physical + swap)
MALLOC: +            0 (    0.0 MB) Bytes released to OS (aka unmapped)
MALLOC:   ------------
MALLOC: =     10092544 (    9.6 MB) Virtual address space used
MALLOC:
MALLOC:             53              Spans in use
MALLOC:              5              Thread heaps in use
MALLOC:           4096              Tcmalloc page size
------------------------------------------------
 */

#define DATA_COUNT (sizeof(SWIFT_TCMALLOC)/sizeof(SWIFT_TCMALLOC[0]))
const static char *SWIFT_TCMALLOC[] = {
    "Bytes in use by application",
    "Bytes in page heap freelist",
    "Bytes in central cache freelist",
    "Bytes in transfer cache freelist",
    "Bytes in thread cache freelists",
    "Bytes in malloc metadata",
    "Actual memory used",
    "Bytes released to OS",
    "Virtual address space used",
    "Spans in use",
    "Thread heaps in use",
    "Tcmalloc page size",
};

/* struct for httpcode counters */
struct status_swift_tcmalloc {
    unsigned long long uba;
    unsigned long long phf;
    unsigned long long ccf;
    unsigned long long trcf;
    unsigned long long thcf;
    unsigned long long mm;
    unsigned long long amu;
    unsigned long long brto;
    unsigned long long vasu;
    unsigned long long siu;
    unsigned long long thiu;
    unsigned long long tps;
} stats;

/* swift register info for tsar */
struct mod_info swift_tcmalloc_info[] = {
    {"   uba", DETAIL_BIT,  0,  STATS_NULL},
    {"   phf", DETAIL_BIT,  0,  STATS_NULL},
    {"   ccf", DETAIL_BIT,  0,  STATS_NULL},
    {"  trcf", DETAIL_BIT,  0,  STATS_NULL},
    {"  thcf", DETAIL_BIT,  0,  STATS_NULL},
    {"    mm", DETAIL_BIT,  0,  STATS_NULL},
    {"   amu", DETAIL_BIT,  0,  STATS_NULL},
    {"  brto", DETAIL_BIT,  0,  STATS_NULL},
    {"  vasu", DETAIL_BIT,  0,  STATS_NULL},
    {"   siu", DETAIL_BIT,  0,  STATS_NULL},
    {"  thiu", DETAIL_BIT,  0,  STATS_NULL},
    {"   tps", DETAIL_BIT,  0,  STATS_NULL},
};
/* opens a tcp or udp connection to a remote host or local socket */
int
my_swift_tcmalloc_net_connect(const char *host_name, int port, int *sd, char *proto)
{
    int                 result;
    struct protoent    *ptrp;
    struct sockaddr_in  servaddr;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp = getprotobyname(proto))) == NULL) {
        if (DEBUG) {
            printf("Cannot map \"%s\" to protocol number\n", proto);
        }
        return 3;
    }

    /* create a socket */
    *sd = socket(PF_INET, (!strcmp(proto, "udp")) ? SOCK_DGRAM : SOCK_STREAM, ptrp->p_proto);

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
                if(DEBUG)
                    printf("Timeout while attempting connection\n");

                break;

            case ENETUNREACH:
                if(DEBUG)
                    printf("Network is unreachable\n");

                break;

            default:
                if(DEBUG)
                    printf("Connection refused or timed out\n");
        }

        return 2;
    }

    return 0;
}

ssize_t
mywrite_swift_tcmalloc(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread_swift_tcmalloc(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
int
read_swift_tcmalloc_value(char *buf, const char *key, unsigned long long *ret)
{
    char *p;

    /* is str match the keywords? */
    if ((p = strstr(buf, key)) != NULL) {
        p = buf;

        /* compute the offset */
        if (strncmp(buf, "MALLOC:  ", sizeof("MALLOC:  ") - 1) == 0) {
            p += sizeof("MALLOC:  ") - 1;

        } else if (strncmp(buf, "MALLOC: +", sizeof("MALLOC: +") - 1) == 0) {
            p += sizeof("MALLOC: +") - 1;

        } else if (strncmp(buf, "MALLOC: =", sizeof("MALLOC: =") - 1) == 0) {
            p += sizeof("MALLOC: =") - 1;
        }

        while (isspace(*p)) {
            p++;
        }

        if (isalnum(*p) == 0) {
            return 0;
        }

        *ret = atoll(p);

        return 1;

    } else {
        return 0;
    }
}

int
parse_swift_tcmalloc_info(char *buf)
{
    int    i;
    char  *line;
    line = strtok(buf, "\n");

    while (line != NULL) {
        for (i = 0; i < DATA_COUNT; i ++) {
            read_swift_tcmalloc_value(line, SWIFT_TCMALLOC[i],
                (unsigned long long *)((char *)&stats + i * sizeof(unsigned long long)));
        };

        line = strtok(NULL, "\n");
    }

    return 0;
}

void
set_swift_tcmalloc_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    for (i = 0; i < mod->n_col; i++) {
        st_array[i] = cur_array[i];
    }
}

int
read_swift_tcmalloc_stat()
{
    char msg[LEN_512];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/mem_stats "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    int len, conn, bytesWritten, fsize = 0;

    if (my_swift_tcmalloc_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
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

    bytesWritten = mywrite_swift_tcmalloc(conn, msg, strlen(msg));

    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_tcmalloc(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 0) {
        close(conn);
        return -1;
    }

    if (parse_swift_tcmalloc_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

void
read_swift_tcmalloc_stats(struct module *mod, char *parameter)
{
    int    retry = 0 , pos = 0;
    char   buf[LEN_4096];
    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 82;
    }
    while (read_swift_tcmalloc_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }

    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            stats.uba,
            stats.phf,
            stats.ccf,
            stats.trcf,
            stats.thcf,
            stats.mm,
            stats.amu,
            stats.brto,
            stats.vasu,
            stats.siu,
            stats.thiu,
            stats.tps
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_tcmalloc", swift_tcmalloc_usage, swift_tcmalloc_info, DATA_COUNT,
        read_swift_tcmalloc_stats, set_swift_tcmalloc_record);
}

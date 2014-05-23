#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL "="
#define DEBUG 0

char *swift_code_usage = "    --swift_code        Swift httpcode";
int mgrport = 82;

/* httpcode string at swiftclient -p 82 mgr:counters */
/*
   http status code 200 = 223291656
   http status code 204 = 54
   http status code 206 = 25473
   http status code 302 = 299
   http status code 304 = 29595013
   http status code 400 = 159
   http status code 403 = 50675
   http status code 404 = 1261657
   http status code 500 = 51
   http status code 502 = 5
   http status code 503 = 229
   http status code 504 = 213
   http status code other = 8982
 */
const static char *SWIFT_CODE[] = {
    "http status code 200",
    "http status code 206",
    "http status code 301",
    "http status code 302",
    "http status code 304",
    "http status code 400",
    "http status code 403",
    "http status code 404",
    "http status code 408",
    "http status code 416",
    "http status code 500",
    "http status code 502",
    "http status code 503",
    "http status code 504",
    "http status code other"
};

/* struct for httpcode counters */
struct status_swift_code {
    unsigned long long code200;
    unsigned long long code206;
    unsigned long long code301;
    unsigned long long code302;
    unsigned long long code304;
    unsigned long long code400;
    unsigned long long code403;
    unsigned long long code404;
    unsigned long long code408;
    unsigned long long code416;
    unsigned long long code500;
    unsigned long long code502;
    unsigned long long code503;
    unsigned long long code504;
    unsigned long long codeother;
} stats;

/* swift register info for tsar */
struct mod_info swift_code_info[] = {
    {"   200", DETAIL_BIT,  0,  STATS_NULL},
    {"   206", DETAIL_BIT,  0,  STATS_NULL},
    {"   301", DETAIL_BIT,  0,  STATS_NULL},
    {"   302", DETAIL_BIT,  0,  STATS_NULL},
    {"   304", DETAIL_BIT,  0,  STATS_NULL},
    {"   400", DETAIL_BIT,  0,  STATS_NULL},
    {"   403", DETAIL_BIT,  0,  STATS_NULL},
    {"   404", DETAIL_BIT,  0,  STATS_NULL},
    {"   408", DETAIL_BIT,  0,  STATS_NULL},
    {"   416", DETAIL_BIT,  0,  STATS_NULL},
    {"   500", DETAIL_BIT,  0,  STATS_NULL},
    {"   502", DETAIL_BIT,  0,  STATS_NULL},
    {"   503", DETAIL_BIT,  0,  STATS_NULL},
    {"   504", DETAIL_BIT,  0,  STATS_NULL},
    {" other", DETAIL_BIT,  0,  STATS_NULL},
};
/* opens a tcp or udp connection to a remote host or local socket */
int
my_swift_code_net_connect(const char *host_name, int port, int *sd, char* proto)
{
    int                 result;
    struct sockaddr_in  servaddr;
    struct protoent    *ptrp;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp = getprotobyname(proto))) == NULL) {
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
mywrite_swift_code(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread_swift_code(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
int
read_swift_code_value(char *buf, const char *key, unsigned long long *ret)
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
parse_swift_code_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_code_value(line, SWIFT_CODE[0], &stats.code200);
        read_swift_code_value(line, SWIFT_CODE[1], &stats.code206);
        read_swift_code_value(line, SWIFT_CODE[2], &stats.code301);
        read_swift_code_value(line, SWIFT_CODE[3], &stats.code302);
        read_swift_code_value(line, SWIFT_CODE[4], &stats.code304);
        read_swift_code_value(line, SWIFT_CODE[5], &stats.code400);
        read_swift_code_value(line, SWIFT_CODE[6], &stats.code403);
        read_swift_code_value(line, SWIFT_CODE[7], &stats.code404);
        read_swift_code_value(line, SWIFT_CODE[8], &stats.code408);
        read_swift_code_value(line, SWIFT_CODE[9], &stats.code416);
        read_swift_code_value(line, SWIFT_CODE[10], &stats.code500);
        read_swift_code_value(line, SWIFT_CODE[11], &stats.code502);
        read_swift_code_value(line, SWIFT_CODE[12], &stats.code503);
        read_swift_code_value(line, SWIFT_CODE[13], &stats.code504);
        read_swift_code_value(line, SWIFT_CODE[14], &stats.codeother);
        line = strtok(NULL, "\n");
    }
    return 0;
}

void
set_swift_code_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        }
    }
}

int
read_swift_code_stat()
{
    char msg[LEN_512];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/counters "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    int len, conn, bytesWritten, fsize = 0;

    if (my_swift_code_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
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

    bytesWritten = mywrite_swift_code(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_code(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_swift_code_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

void
read_swift_code_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096];
    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if(!mgrport){
        mgrport = 82;
    }
    while (read_swift_code_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            stats.code200,
            stats.code206,
            stats.code301,
            stats.code302,
            stats.code304,
            stats.code400,
            stats.code403,
            stats.code404,
            stats.code408,
            stats.code416,
            stats.code500,
            stats.code502,
            stats.code503,
            stats.code504,
            stats.codeother
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_code", swift_code_usage, swift_code_info, 15, read_swift_code_stats, set_swift_code_record);
}

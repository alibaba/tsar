#define _GNU_SOURCE

#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"
#include <string.h>
#include <stdio.h>

#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 81
#define EQUAL "="
#define DEBUG 0

char *swift_domain_usage = "    --swift_domain             Swift domain info";
int mgrport = 81;

/* httpcode string at swiftclient -p 81 mgr:domain_list */
/*
 * DOMAIN                        HIT(%)         REQ        MISS       rt         fwd
 * cdn.hc.org                     0.11%     3331664     3327927     0.01     3327927
 * detail.tmall.hk                0.00%           0           0     0.00           0
 * detail.tmall.com              51.78%     3512541     1693730    12.47     1695351
 * item.tmall.com                32.94%          85          57    19.31          57
 * d.life.taobao.com              0.00%           3           3    53.67           6
 * d.tongcheng.taobao.com         0.00%           0           0     0.00           0
 * item.taobao.com               28.56%      327813      234176    17.25      525541
 * edyna.item.taobao.com          0.00%           0           0     0.00           0
 * default.swift                  0.00%           0           0     0.00           0
 */
static char *SWIFT_DOMAIN[] = {
    "detail.tmall.com",
    "item.tmall.com",
    "item.taobao.com",
    "edyna.item.taobao.com",
};

static int stats_count = sizeof(SWIFT_DOMAIN)/sizeof(char *);
static char **swift_domain = SWIFT_DOMAIN;
static char *swift_domain_array[4096];

/* struct for http domain */
unsigned long long swift_domain_stats[1024][3];

/* swift register info for tsar */
struct mod_info swift_domain_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {"   hit", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", DETAIL_BIT,  0,  STATS_NULL}
};

/* opens a tcp or udp connection to a remote host or local socket */
static int my_swift_domain_net_connect(const char *host_name, int port, int *sd, char* proto)
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

static ssize_t mywrite_swift_code(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

static ssize_t myread_swift_code(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
static int read_swift_domain_value(char *buf, const char *key, unsigned long long *r1,
                                   unsigned long long *r2, unsigned long long *r3)
{
    int   i;
    char  *token;

    token = strtok(buf, " \t");

    if (token == NULL || strcmp(token, key) != 0)
        return 1;

    //fprintf(stderr, "buf: %s, domain: %s\n", token, key);
    /* is str match the keywords? */
    for(i = 0; token && i < 4; i ++) {
        token = strtok(NULL, " \t");

        if (!token)
            break;

        //fprintf(stderr, "token: %s\n", token);
        if (i == 1) *r1 = strtoll(token, NULL, 10);
        if (i == 2) *r2 = strtoll(token, NULL, 10);
        if (i == 3) *r3 = strtoll(token, NULL, 10);
    }
    //fprintf(stderr, "domain: %s, %lld, %lld, %lld\n", key, *r1, *r2, *r3);

    return 1;
}

static int parse_swift_code_info(char *buf)
{
    char *line, *p, *pos, *nline;
    int  i, len;

    pos = buf;
    len = strlen(buf);

    while (pos < buf + len) {
        if ((p = strchr(pos, '\n')) == NULL) {
            line = strdup(pos);
            pos = buf + len;
        } else {
            line = strndup(pos, (size_t)(p - pos));
            pos = p + 1;
        }
        //fprintf(stderr, "line: %s\n", line);
        for (i = 0; i < stats_count; i ++) {
            nline = strdup(line);
            read_swift_domain_value(nline, swift_domain[i],
                                    &swift_domain_stats[i][0],
                                    &swift_domain_stats[i][1],
                                    &swift_domain_stats[i][2]);
            free(nline);
        }
        free(line);
    }
    return 0;
}

static void set_swift_domain_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] - pre_array[0]) * 1.0 / inter;
    } else {
        st_array[0] = 0;
    }
    if (cur_array[1] >= pre_array[1] && cur_array[0] > pre_array[0]) {
        st_array[1] = (cur_array[1] - pre_array[1]) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[1] = (1 - st_array[1]) * 100;
    } else {
        st_array[1] = 0;
    }
    if (cur_array[2] >= 0) {
        st_array[2] = cur_array[2];
    } else {
        st_array[2] = 0;
    }
}

static int read_swift_code_stat()
{
    char msg[LEN_512];
    char buf[LEN_4096];
    sprintf(msg,
            "GET cache_object://localhost/domain_list "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    int len, conn = 0, bytesWritten, fsize = 0;

    if (my_swift_domain_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
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

    while ((len = myread_swift_code(conn, buf, sizeof(buf))) > 0) {
        fsize += len;
    }

    if (parse_swift_code_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

static void swift_domain_init(char *parameter)
{
    FILE    *fp = fopen(parameter, "r");
    char    *line = NULL, *domain;
    size_t  size = 1024;
    ssize_t ret = 0;
    int     i = 0;

    if (fp == NULL) {
        mgrport = 8889;
        return;
    }

    line = calloc(1, size);
    while((ret = getline(&line, &size, fp)) > 0) {
        if (ret > 5 && strncasecmp("port=", line, 5) == 0) {
            mgrport = atoi(line + 5);
            if (!mgrport){
                mgrport = 81;
            }
        } else if (ret > 7 && strncasecmp("domain=", line, 7) == 0) {
            domain = line + 7;
            line[ret - 1] = '\0';
            if (i < 4096)
                swift_domain_array[i ++] = strdup(domain);
        }
    }

    swift_domain = swift_domain_array;
    stats_count = i;

    if (line)
        free(line);
}

static void swift_domian_free()
{
    int i;

    for (i = 0; i < 4096; i ++) {
        if (swift_domain_array[i])
            free(swift_domain_array[i]);
    }
}

static void read_swift_domain_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_1024];
    int    i;

    memset(&swift_domain_stats, 0, sizeof(swift_domain_stats));

    swift_domain_init(parameter);

    while (read_swift_code_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }

    for (i = 0; i < stats_count; i ++) {
        pos += sprintf(buf + pos, "%s=%lld,%lld,%lld",
                swift_domain[i],
                swift_domain_stats[i][0],
                swift_domain_stats[i][1],
                swift_domain_stats[i][2]);
        pos += sprintf(buf + pos, ITEM_SPLIT);
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);

    swift_domian_free();
}

void mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_domain", swift_domain_usage, swift_domain_info, 3, read_swift_domain_stats, set_swift_domain_record);
}

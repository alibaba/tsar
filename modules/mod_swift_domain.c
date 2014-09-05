#define _GNU_SOURCE

#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL "="
#define DEBUG 0
#define NUM_DOMAIN_MAX 4096
#define DOMAIN_LIST_DELIM ", \t"

char *swift_domain_usage = "    --swift_domain      Swift domain info";
int mgrport = 82;

/* httpcode string at swiftclient -p 82 mgr:domain_list */
/*
 * DOMAIN                        HIT(%)         REQ        MISS       rt         fwd
 * cdn.hc.org                     0.11%     3331664     3327927     0.01     3327927
 * detail.tmall.hk                0.00%           0           0     0.00           0
 * detail.tmall.com              51.78%     3512541     1693730    12.47     1695351
 * item.tmall.com                32.94%          85          57    19.31          57
 * d.life.taobao.com              0.00%           3           3    53.67           6
 * d.tongcheng.taobao.com         0.00%           0           0     0.00           0
 * item.taobao.com               28.56%      327823      234176    17.25      525541
 * edyna.item.taobao.com          0.00%           0           0     0.00           0
 * default.swift                  0.00%           0           0     0.00           0
 */
static char *SWIFT_DOMAIN[] = {
    "detail.tmall.com",
    "item.tmall.com",
    "item.taobao.com",
    "edyna.item.taobao.com",
};

typedef struct domain_id_pair {
    const char *domain;
    int         id;
} domain_id_pair;

int num_domain = 0;
static int stats_count = sizeof(SWIFT_DOMAIN)/sizeof(char *);
static char **swift_domain = SWIFT_DOMAIN;
static char *swift_domain_array[NUM_DOMAIN_MAX];
static domain_id_pair domain_to_id[NUM_DOMAIN_MAX];

/* struct for http domain */
long long swift_domain_stats[1024][3];

/* swift register info for tsar */
struct mod_info swift_domain_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {"   hit", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", DETAIL_BIT,  0,  STATS_NULL}
};

static int cmp(const void *a, const void *b)
{
    domain_id_pair *pa = (domain_id_pair *)a,
                   *pb = (domain_id_pair *)b;

    return strcmp(pa->domain, pb->domain);
}

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

static void read_swift_domain_value(char *buf,
        long long *r1, long long *r2, long long *r3)
{
    int       ret;
    char      token[1024], hit_dumb[32];
    long long req, miss, rt;

    ret = sscanf(buf, "%s%s%lld%lld%lld", token, hit_dumb, &req, &miss, &rt);
    if (ret != 5) {
        return;
    }

    if (rt < 0)
        rt = 0;

    *r1 += req;
    *r2 += miss;
    *r3 += rt;

    *r1 = *r1 < 0LL ? 0LL : *r1;
    *r2 = *r2 < 0LL ? 0LL : *r2;
    *r3 = *r3 < 0LL ? 0LL : *r3;
}

/**
 * Try to parse the response in @buf provided by
 * swift client. Return 0 on success, -1 on error.
 */
static int parse_swift_code_info(char *buf, size_t buflen)
{
    char           *line, *p, *pos, token[1024];
    int             len, id;
    domain_id_pair *pair, key;

    pos = buf;
    len = strlen(buf);

    while (pos < buf + buflen) {
        if ((p = strchr(pos, '\n')) == NULL) {
            /* no newline, ill formatted */
            return -1;
        }

        line = strndup(pos, (size_t)(p - pos));
        pos = p + 1;

        /* looking for the id this domain has */
        sscanf(line, "%s", token);
        key.domain = token;
        pair = bsearch(&key, domain_to_id, num_domain,
                sizeof(domain_to_id[0]), cmp);

        if (pair == NULL) {
            free(line);
            continue;
        }

        id = pair->id;

        read_swift_domain_value(line, &swift_domain_stats[id][0],
                                      &swift_domain_stats[id][1],
                                      &swift_domain_stats[id][2]);

        free(line);
    }

    return 0;
}

/**
 * xxx_array[0]: req
 * xxx_array[1]: miss
 * xxx_array[2]: rt (actually serving time)
 */
static void set_swift_domain_record(struct module *mod, double st_array[],
     U_64 pre_array[], U_64 cur_array[], int inter)
{
    /* qps */
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] - pre_array[0]) * 1.0 / inter;
    } else {
        st_array[0] = 0.0;
    }

    /* hit ratio */
    if (cur_array[1] >= pre_array[1] && cur_array[0] > pre_array[0]) {
        st_array[1] = (cur_array[1] - pre_array[1]) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[1] = (1.0 - st_array[1]) * 100.0;
    } else {
        st_array[1] = 0.0;
    }

    /* rt */
    if (cur_array[0] > pre_array[0] && cur_array[2] >= pre_array[2]) {
        st_array[2] = (cur_array[2] - pre_array[2]) * 1.0 / (cur_array[0] - pre_array[0]);
    } else {
        st_array[2] = 0.0;
    }
}

static int read_swift_code_stat()
{
    char           msg[LEN_512], buf[1024*1024];
    int            len, conn = 0, bytes_written, fsize = 0, flags;
    struct timeval timeout = {10, 0};

    sprintf(msg,
            "GET cache_object://localhost/domain_list "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n");

    if (my_swift_domain_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    if ((flags = fcntl(conn, F_GETFL, 0)) < 0) {
        close(conn);
        return -1;
    }

    if (fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(conn);
        return -1;
    }

    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

    bytes_written = mywrite_swift_code(conn, msg, strlen(msg));
    if (bytes_written < 0) {
        close(conn);
        return -2;
    } else if (bytes_written != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift_code(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    buf[fsize] = '\0';

    if (parse_swift_code_info(buf, fsize) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

static void swift_domain_init(char *parameter)
{
    FILE    *fp;
    char    *line = NULL, *domain, *token, *s;
    size_t   size = 1024;
    ssize_t  ret = 0;
    int      domain_id = 0, first;

    num_domain = 0;

    fp = fopen(parameter, "r");
    if (fp == NULL) {
        mgrport = 8889;
        return;
    }

    line = calloc(1, size);
    while ((ret = getline(&line, &size, fp)) > 0) {
        if (ret > 5 && strncasecmp("port=", line, 5) == 0) {
            mgrport = atoi(line + 5);
            if (!mgrport) {
                mgrport = 82;
            }
        } else if (ret > 7 && strncasecmp("domain=", line, 7) == 0) {
            line[ret - 1] = '\0';
            domain = line + 7;

            first = 1;
            token = strtok(domain, DOMAIN_LIST_DELIM);
            while (token != NULL) {
                assert(num_domain < NUM_DOMAIN_MAX);

                s = strdup(token);

                if (first) {
                    swift_domain_array[domain_id] = s;
                    first = 0;
                }

                domain_to_id[num_domain].domain = s;
                domain_to_id[num_domain].id = domain_id;
                ++num_domain;

                token = strtok(NULL, DOMAIN_LIST_DELIM);
            }

            ++domain_id;
        }
    }

    swift_domain = swift_domain_array;
    stats_count = domain_id;

    qsort(domain_to_id, num_domain, sizeof(domain_to_id[0]), cmp);

    if (line) {
        free(line);
    }
}

static void swift_domian_free()
{
    int i;

    for (i = 0; i < NUM_DOMAIN_MAX; i ++) {
        if (swift_domain_array[i])
            free(swift_domain_array[i]);
    }
}

static void read_swift_domain_stats(struct module *mod, char *parameter)
{
    int  i, retry = 0, pos = 0;
    char buf[LEN_4096];

    memset(&swift_domain_stats, 0, sizeof(swift_domain_stats));

    swift_domain_init(parameter);

    while (read_swift_code_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }

    for (i = 0; i < stats_count; i++) {
        assert(swift_domain_stats[i][0] >= 0
                && swift_domain_stats[i][1] >= 0
                && swift_domain_stats[i][2] >= 0);

        pos += sprintf(buf + pos, "%s=%lld,%lld,%lld",
                                  swift_domain_array[i],
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
    register_mod_fileds(mod, "--swift_domain", swift_domain_usage,
                        swift_domain_info, 3, read_swift_domain_stats,
                        set_swift_domain_record);
}

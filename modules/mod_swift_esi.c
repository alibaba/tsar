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

char *swift_esi_usage = "    --swift_esi         Swift ESI info";
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
static char **swift_esi = SWIFT_DOMAIN;
static char *swift_esi_array[NUM_DOMAIN_MAX];
static domain_id_pair domain_to_id[NUM_DOMAIN_MAX];

/* struct for http domain */
long long swift_esi_stats[1024][5];

/* swift register info for tsar */
struct mod_info swift_esi_info[] = {
    {"  eqps", DETAIL_BIT,  0,  STATS_NULL},
    {" emiss", DETAIL_BIT,  0,  STATS_NULL},
    {"  ehit", DETAIL_BIT,  0,  STATS_NULL},
    {" ecomb", DETAIL_BIT,  0,  STATS_NULL},
    {" pload", DETAIL_BIT,  0,  STATS_NULL}
};

static int cmp(const void *a, const void *b)
{
    domain_id_pair *pa = (domain_id_pair *)a,
                   *pb = (domain_id_pair *)b;

    return strcmp(pa->domain, pb->domain);
}

/* opens a tcp or udp connection to a remote host or local socket */
static int my_swift_esi_net_connect(const char *host_name, int port, int *sd, char* proto)
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

static void read_swift_esi_value(char *buf,
        long long *r1, long long *r2, long long *r3, long long *r4, long long *r5)
{
    int       ret;
    char      token[1024][11];
    long long ereq, emiss, ehit, ecomb, preload;

    memset(token, 0, sizeof(token));
    ret = sscanf(buf, "%s%s%s%s%s%s%s%s%s%s%s%lld%lld%lld%lld%lld", token[0], token[1], token[2],
                 token[3], token[4], token[5], token[6], token[7], token[8], token[9], token[10],
                 &ereq, &emiss, &ehit, &ecomb, &preload);
    if (ret != 16) {
        return;
    }

    if (ereq < 0)
        ereq = 0;
    if (emiss < 0)
        emiss = 0;
    if (ehit < 0)
        ehit = 0;
    if (ecomb < 0)
        ecomb = 0;
    if (preload < 0)
        preload = 0;

    *r1 += ereq;
    *r2 += emiss;
    *r3 += ehit;
    *r4 += ecomb;
    *r5 += preload;

    *r1 = *r1 < 0LL ? 0LL : *r1;
    *r2 = *r2 < 0LL ? 0LL : *r2;
    *r3 = *r3 < 0LL ? 0LL : *r3;
    *r4 = *r4 < 0LL ? 0LL : *r4;
    *r5 = *r5 < 0LL ? 0LL : *r5;
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
    int            found[1024] = {0};

    pos = buf;
    len = strlen(buf);

    while (pos < buf + buflen) {
        if ((p = strchr(pos, '\n')) == NULL) {
            /* no newline, ill formatted */
            return -1;
        } else if (p == pos + 1 || p == pos) {
            pos = p + 1;
            continue;
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
        if (found[id] == 1) {
            free(line);
            continue;
        }
        found[id] = 1;

        read_swift_esi_value(line, &swift_esi_stats[id][0],
                                   &swift_esi_stats[id][1],
                                   &swift_esi_stats[id][2],
                                   &swift_esi_stats[id][3],
                                   &swift_esi_stats[id][4]);

        free(line);
    }

    return 0;
}

/**
 * xxx_array[0]: esi_request
 * xxx_array[1]: esi_miss
 * xxx_array[2]: esi_hit
 * xxx_array[3]: esi_combine
 */
static void set_swift_esi_record(struct module *mod, double st_array[],
     U_64 pre_array[], U_64 cur_array[], int inter)
{
    /* qps */
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] - pre_array[0]) * 1.0 / inter;
    } else {
        st_array[0] = 0.0;
    }

    if (cur_array[2] - pre_array[2] < cur_array[4] - pre_array[4]) {
        st_array[1] = 0.0;
        st_array[2] = 0.0;
        st_array[3] = 0.0;
        st_array[4] = 0.0;
        return ;
    }

    /* miss ratio */
    if (cur_array[1] >= pre_array[1] && cur_array[0] > pre_array[0]) {
        st_array[1] = (cur_array[1] - pre_array[1] + (cur_array[4] - pre_array[4])) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[1] = st_array[1] * 100.0;
    } else {
        st_array[1] = 0.0;
    }

    /* hit */
    if (cur_array[0] > pre_array[0] && cur_array[2] >= pre_array[2]) {
        st_array[2] = (cur_array[2] - pre_array[2] - (cur_array[4] - pre_array[4])) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[2] = st_array[2] * 100.0;
    } else {
        st_array[2] = 0.0;
    }
    /* comb */
    if (cur_array[0] > pre_array[0] && cur_array[3] >= pre_array[3]) {
        st_array[3] = (cur_array[3] - pre_array[3]) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[3] = st_array[3] * 100.0;
    } else {
        st_array[3] = 0.0;
    }
    /* comb */
    if (cur_array[0] > pre_array[0] && cur_array[4] >= pre_array[4]) {
        st_array[4] = (cur_array[4] - pre_array[4]) * 1.0 / (cur_array[0] - pre_array[0]);
        st_array[4] = st_array[4] * 100.0;
    } else {
        st_array[4] = 0.0;
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

    if (my_swift_esi_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
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

static void swift_esi_init(char *parameter)
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
                    swift_esi_array[domain_id] = s;
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

    swift_esi = swift_esi_array;
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
        if (swift_esi_array[i])
            free(swift_esi_array[i]);
    }
}

static void read_swift_esi_stats(struct module *mod, char *parameter)
{
    int  i, retry = 0, pos = 0;
    char buf[LEN_4096];

    memset(&swift_esi_stats, 0, sizeof(swift_esi_stats));

    swift_esi_init(parameter);

    while (read_swift_code_stat() < 0 && retry < RETRY_NUM) {
        retry++;
    }

    for (i = 0; i < stats_count; i++) {
        assert(swift_esi_stats[i][0] >= 0
                && swift_esi_stats[i][1] >= 0
                && swift_esi_stats[i][2] >= 0
                && swift_esi_stats[i][3] >= 0
                && swift_esi_stats[i][4] >= 0);

        pos += sprintf(buf + pos, "%s=%lld,%lld,%lld,%lld,%lld",
                                  swift_esi_array[i],
                                  swift_esi_stats[i][0],
                                  swift_esi_stats[i][1],
                                  swift_esi_stats[i][2],
                                  swift_esi_stats[i][3],
                                  swift_esi_stats[i][4]);

        pos += sprintf(buf + pos, ITEM_SPLIT);
    }

    buf[pos] = '\0';
    set_mod_record(mod, buf);

    swift_domian_free();
}

void mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift_esi", swift_esi_usage,
                        swift_esi_info, 5, read_swift_esi_stats,
                        set_swift_esi_record);
}

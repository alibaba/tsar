#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "tsar.h"


#define RETRY_NUM 3

char *squid_usage = "    --squid             Squid utilities";

struct stats_squid {
    int usable;
    struct squid_counters
    {
        struct counters_client {
            unsigned long long http_requests;
            unsigned long long http_hits;
            unsigned long long http_kbytes_out;
            unsigned long long http_hit_kbytes_out;

        } cc;
        struct counters_server {
            unsigned long long all_requests;
            unsigned long long all_kbytes_in;
            unsigned long long all_kbytes_out;
        } cs;
    } sc;
    struct squid_info {
        struct mem_usage {
            unsigned long long mem_total;
            unsigned long long mem_free;
            unsigned long long mem_size;
        } mu;
        struct fd_usage {
            unsigned int fd_used;
            unsigned int fd_queue;
        } fu;
        struct info_internal_data {
            unsigned int entries;
            unsigned int memobjs;
            unsigned int hotitems;
        } iid;
        struct squid_float {
            unsigned long long meanobjsize;
            unsigned long long responsetime;
            unsigned long long disk_hit;
            unsigned long long mem_hit;
            unsigned long long http_hit_rate;
            unsigned long long byte_hit_rate;
        } sf;
    } si;
};


#define STATS_SQUID_SIZE (sizeof(struct stats_squid))
#define MAXSQUID 10

/*
 * here we defined a pointer to a array of structure
 * we should call the member like:
 *
 *              (*(s_st_squid + p_idx))[idx].member
 *
 */
//typedef struct __stats_squid (* stats_squid)[2];

struct stats_squid  s_st_squid[MAXSQUID];

struct p_squid_info {
    struct squid_counters *scp;
    struct squid_info * sip;
};

#define DIGITS "0123456789"
#define LEFT 0
#define RIGHT 1
#define SQUIDDIGITS "0123456789-"
#define MAXINT 2147483647

char *key[] = {
    "client_http.requests",
    "client_http.hits",
    "client_http.kbytes_out",
    "client_http.hit_kbytes_out"
};

char *key_info[] = {
    "Total in use",
    "Total free",
    "Total size",
    "Number of file desc currently in use",
    "Files queued for open",
    "StoreEntries",
    "StoreEntries with MemObjects",
    "Hot Object Cache Items",
    "Mean Object Size",
};

char *key_float[] = {
    "Average HTTP respone time",
    "Mean Object Size:",
    "Request Memory Hit Ratios:",
    "Request Filesystem Hit Ratios:",
    "Request Hit Ratios:",
    "Byte Hit Ratios:",
    "Request Disk Hit Ratios:",
    "HTTP Requests (All):",
};

char *
a_trim(char *str, int len)
{
    int    i = 0;
    char  *dest = NULL, *l_str;
    dest = (char *)malloc(len);
    if (dest == NULL ){
        return NULL;
    }
    l_str = str;
    if (l_str == NULL) {
        free(dest);
        return NULL;
    }
    while (*l_str++ != '\0' && len--) {
        if (*l_str != ' ' && *l_str != '\t') {
            dest[i++] = *l_str;
        }
    }
    dest[i] = '\0';
    return dest;
}

int
read_a_int_value(char *buf, char *key, unsigned int *ret, int type)
{
    int    k;
    char  *tmp;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {
        /* is number befor the key string? */
        if (type == LEFT) {
            tmp = buf;
        }
        /* compute the offset */
        k = strcspn(tmp, DIGITS);
        sscanf(tmp + k, "%d", ret);
        return 1;

    } else {
        return 0;
    }
}

int
read_a_long_long_value(char *buf, char *key, unsigned long long *ret, int type)
{
    int    k;
    char  *tmp;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {

        /* is number befor the key string? */
        if (type == LEFT) {
            tmp = buf;
        }
        /* compute the offset */
        k = strcspn(tmp, DIGITS);
        sscanf(tmp + k, "%lld", ret);
        return 1;

    } else {
        return 0;
    }
}

/*add for squidclient when counter is nagtive*/
int
read_a_long_long_value_squid(char *buf, char *key, unsigned long long *ret, int type)
{
    int    k;
    char  *tmp;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {

        /* is number befor the key string? */
        if (type == LEFT) {
            tmp = buf;
        }
        /* compute the offset */
        k = strcspn(tmp, SQUIDDIGITS);
        sscanf(tmp + k, "%lld", ret);
        if (*ret > MAXINT) {
            *ret += MAXINT;
        }
        return 1;

    } else {
        return 0;
    }
}

int
read_a_float_value(char *buf, char *key, unsigned long long *ret, int type, int len)
{
    int    k;
    int    r, l;
    char  *tmp;
    char  *tmp2;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {

        /* is number befor the key string? */
        if (type == LEFT) {
            tmp = buf;
        }
        /* skip the trial number like 5min:*/
        if ((tmp2 = strstr(tmp, "min")) != NULL) {
            tmp = tmp2;
        }
        /* compute the offset */
        k = strcspn(tmp, DIGITS);
        sscanf(tmp + k, "%d.%d", &r, &l);
        *ret = (unsigned long long)r * len + l;
        return 1;

    } else {
        return 0;
    }
}

void
collect_cnts(char *l, struct squid_counters *sc)
{
    read_a_long_long_value_squid(l, key[0],
            &sc->cc.http_requests, RIGHT);

    read_a_long_long_value(l, key[1],
            &sc->cc.http_hits, RIGHT);

    read_a_long_long_value(l, key[2],
            &sc->cc.http_kbytes_out, RIGHT);

    read_a_long_long_value(l, key[3],
            &sc->cc.http_hit_kbytes_out, RIGHT);
}


void
collect_info(char *l, struct squid_info *si)
{
    read_a_long_long_value(l, key_info[0],
            &si->mu.mem_total, RIGHT);

    read_a_long_long_value(l, key_info[1],
            &si->mu.mem_free, RIGHT);

    read_a_long_long_value(l, key_info[2],
            &si->mu.mem_size, RIGHT);

    read_a_int_value(l, key_info[3],
            &si->fu.fd_used, RIGHT);

    read_a_int_value(l, key_info[4],
            &si->fu.fd_queue, RIGHT);

    /* here don't confuse the order */
    if (read_a_int_value(l, key_info[6],
                &si->iid.memobjs, LEFT))
    {
        return;
    }

    read_a_int_value(l, key_info[5],
            &si->iid.entries, LEFT);

    read_a_int_value(l, key_info[7],
            &si->iid.hotitems, LEFT);

    read_a_float_value(l, key_float[0],
            &si->sf.responsetime,
            RIGHT, 100);

    read_a_float_value(l, key_float[1],
            &si->sf.meanobjsize,
            RIGHT, 100);
    read_a_float_value(l, key_float[3],
            &si->sf.disk_hit,
            RIGHT, 10);
    read_a_float_value(l, key_float[2],
            &si->sf.mem_hit,
            RIGHT, 10);
    read_a_float_value(l, key_float[4],
            &si->sf.http_hit_rate,
            RIGHT, 10);
    read_a_float_value(l, key_float[5],
            &si->sf.byte_hit_rate,
            RIGHT, 10);
    read_a_float_value(l, key_float[6],
            &si->sf.disk_hit,
            RIGHT, 10);
    read_a_float_value(l, key_float[7],
            &si->sf.responsetime,
            RIGHT, 100000);

}

int port_list[MAXSQUID] = {0};

static int squid_nr = 0;
static int live_squid_nr = 0;

void
count_squid_nr()
{
    DIR            *dp;
    char           *s_token, *e_token;
    static char     tmp_s_port[32] = {0};
    struct dirent  *dirp;
    squid_nr = 0;
    if (!(dp = opendir("/etc/squid/"))) {
        return;
    }
    while ((dirp = readdir(dp)))
    {
        s_token = strstr(dirp->d_name, "squid.");
        if (s_token) {
            e_token = strstr(s_token + 6, ".conf");
            if (e_token && *(e_token + 5) == '\0') {
                memset(tmp_s_port, 0, sizeof(tmp_s_port));
                memcpy(tmp_s_port, s_token + 6, e_token - s_token - 6);
                port_list[squid_nr++] = atoi(tmp_s_port);
            }
        }
    }
    if (squid_nr > MAXSQUID) {
        squid_nr = MAXSQUID;
    }
    closedir(dp);
}


ssize_t
mywrite(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

ssize_t
myread(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

int
client_comm_connect(int sock, const char *dest_host, u_short dest_port, struct timeval *tvp)
{
    int                    flags, res;
    fd_set                 fdr, fdw;
    struct  timeval        timeout;
    struct sockaddr_in     to_addr;
    const struct hostent  *hp = NULL;

    /* set socket fd noblock */
    if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    /* Set up the destination socket address for message to send to. */
    if (hp == NULL) {
        to_addr.sin_family = AF_INET;

        if ((hp = gethostbyname(dest_host)) == 0)
            return (-1);
        memcpy(&to_addr.sin_addr, hp->h_addr, hp->h_length);
        to_addr.sin_port = htons(dest_port);
    }
    if (connect(sock, (struct sockaddr *) &to_addr, sizeof(struct sockaddr_in)) != 0) {
        if (errno != EINPROGRESS) {
            return -1;

        } else {
            goto select;
        }

    } else {
        return -1;
    }
select:
    FD_ZERO(&fdr);
    FD_ZERO(&fdw);
    FD_SET(sock, &fdr);
    FD_SET(sock, &fdw);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    res = select(sock + 1, &fdr, &fdw, NULL, &timeout);
    if (res < 0) {
        return -1;

    } else if (res == 0) {
        return -1;

    } else {
        return 1;
    }
}

int
parse_squid_info(char *buf, char *cmd, struct p_squid_info *p_si)
{
    char *line;
    line = strtok(buf, "\n");

    while (line != NULL) {
        if (!strcmp(cmd, "counters")) {
            collect_cnts(line, p_si->scp);

        } else if (!strcmp(cmd, "info")) {
            collect_info(line, p_si->sip);

        } else {
            fprintf(stderr, "unknown command\n");
            return -1;
        }
        line = strtok(NULL, "\n");
    }

    if (!strcmp(cmd, "counters") && p_si->scp->cc.http_requests == 0) {
        return -1;
    }
    if (!strcmp(cmd, "info") && p_si->sip->sf.responsetime == 0) {
        /* return -1;*/
    }
    return 0;
}

int
__get_squid_info(char *squidoption, char *squidcmd, int port, int index)
{
    char buf[LEN_4096];
    char *hostname = "localhost";
    int len, conn, bytesWritten, fsize = 0;
    struct p_squid_info psi = {&s_st_squid[index].sc, &s_st_squid[index].si};

    if ((conn = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        close(conn);
        return -1;
    }
    if (client_comm_connect(conn, hostname, port, NULL) < 0) {
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

    bytesWritten = mywrite(conn, squidcmd, strlen(squidcmd));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(squidcmd)) {
        close(conn);
        return -3;
    }
    while ((len = myread(conn, buf, sizeof(buf) - fsize -1)) > 0) {
        fsize += len;
    }
    buf[fsize] = '\0';
    /* read error */
    if (fsize < 1000) {
        close(conn);
        return -1;
    }

    if (parse_squid_info(buf, squidoption, &psi) < 0) {
        close(conn);
        return -1;
    }
    close(conn);
    return 0;

}

int
__read_squid_stat(int port, int index)
{
    int i;
    /* fullfil the raw infomation here
       for each port */
    char *options[2] = {"info", "counters"};
    char msg[2][LEN_512];

    /* we have two command 'info' & 'counters' to run */
    for (i = 0; i < 2; i++) {
        sprintf(msg[i],
                "GET cache_object://localhost/%s "
                "HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Accept:*/*\r\n"
                "Connection: close\r\n\r\n",
                options[i]);
        if (__get_squid_info(options[i], msg[i], port, index) < 0) {
            return -1;
        }
    }
    return 0;
}

int
store_single_port(char *buf, char *itemname, int i)
{
    int len = 0;
    len += sprintf(buf, "%s=", itemname);
    /* print single port info to buffer here */
    len += sprintf(buf + len,
            "%llu,%llu,%llu,%llu,%llu,%llu,"
            "%u,%u,%u,%u,%u,%llu,%u,%u",
            s_st_squid[i].sc.cc.http_requests,
            s_st_squid[i].si.sf.responsetime,
            s_st_squid[i].si.sf.http_hit_rate,
            s_st_squid[i].si.sf.byte_hit_rate,
            s_st_squid[i].si.sf.disk_hit,
            s_st_squid[i].si.sf.mem_hit,
            s_st_squid[i].si.fu.fd_used,
            s_st_squid[i].si.fu.fd_queue,
            s_st_squid[i].si.iid.entries,
            s_st_squid[i].si.iid.memobjs,
            s_st_squid[i].si.iid.hotitems,
            s_st_squid[i].si.sf.meanobjsize,
            squid_nr,
            live_squid_nr);
    return len;
}

void
read_squid_stat(struct module *mod, char *parameter)
{
    int i, pos = 0;
    char buf[LEN_4096] = {0};
    char itemname[LEN_4096] = {0};
    live_squid_nr = 0;

    count_squid_nr();
    if (squid_nr == 0) {
        if (atoi(parameter) != 0) {
            port_list[0] = atoi(parameter);  
            squid_nr = 1;
        } else {
            port_list[0] = 3128;
            squid_nr = 1;
        }
    }

    memset(s_st_squid, 0, STATS_SQUID_SIZE * MAXSQUID);
    /*get the live squid number*/
    for (i = 0; i < squid_nr; i++) {
        int retry = 0;
        /* read on each port and retry to get squidclient for 3 times*/
        while (__read_squid_stat(port_list[i], i) < 0 && retry < RETRY_NUM) {
            retry++;
        }
        if (retry == RETRY_NUM) {
            continue;
        }
        s_st_squid[i].usable = TRUE;
        live_squid_nr++;
    }

    /* traverse the port list */
    for (i = 0; i < squid_nr; i++) {
        if (!s_st_squid[i].usable) {
            continue;
        }
        /* generate the item name */
        int n = sprintf(itemname, "port%d", port_list[i]);
        itemname[n] = '\0';

        /* print log to buffer */
        pos += store_single_port(buf + pos, itemname, i);
        /* print a seperate char */
        pos += sprintf(buf + pos, ITEM_SPLIT);
    }
    if (pos && squid_nr == live_squid_nr) {
        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


static void
set_squid_record(struct module *mod, double st_array[], U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    /* set st record */
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] - pre_array[0]) / inter;

    } else {
        st_array[0] = 0;
    }
    st_array[1] = cur_array[1] / 100.0;
    for (i = 2; i <=  5; i++) st_array[i] = cur_array[i] / 10.0;
    for (i = 6; i <= 10; i++) st_array[i] = cur_array[i];
    st_array[11] = (cur_array[11] << 10) / 100.0;
    st_array[12] = cur_array[12];
    st_array[13] = cur_array[13];
}

static struct mod_info s_info[] = {
    {"   qps", SUMMARY_BIT,  MERGE_SUM,  STATS_SUB_INTER},
    {"    rt", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {" r_hit", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {" b_hit", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {" d_hit", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {" m_hit", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {"fdused", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {" fdque", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {"  objs", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {" inmem", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {"   hot", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {"  size", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {"totalp", DETAIL_BIT, MERGE_AVG,  STATS_NULL},
    {" livep", DETAIL_BIT, MERGE_AVG,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--squid", squid_usage, s_info, 14, read_squid_stat, set_squid_record);
}

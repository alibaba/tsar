#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"
#include "mod_swift.h"

#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82

char *swift_usage = "    --swift             Swift object storage infomation";
int mgrport = 82;

/* string at swiftclient -p 82 mgr:info */
/*
 * Average HTTP respone time:      5min: 11.70 ms, 60min: 10.06 ms
 * Request Hit Ratios:     5min: 95.8%, 60min: 95.7%
 * Byte Hit Ratios:        5min: 96.6%, 60min: 96.6%
 * UP Time:        247256.904 seconds
 * CPU Time:       23487.042 seconds
 * StoreEntries           : 20776287
 * client_http.requests = 150291472
 * client_http.hits = 0
 * client_http.total_svc_time = 1998366283
 * client_http.bytes_in = 6380253436
 * client_http.bytes_out = 5730106537327
 */
const static char *SWIFT_STORE[] = {
    "client_http.requests",
    "client_http.bytes_in",
    "client_http.bytes_out",
    "client_http.total_svc_time",
    "client_http.hits",
    "StoreEntries",
    "Number of clients accessing cache",
    "client_http.accepts",
    "client_http.conns",
    "server_http.bytes_in",
};

/* struct for swift counters */
struct status_swift {
    unsigned long long requests;
    unsigned long long hits;
    unsigned long long total_svc_time;
    unsigned long long b_hit;
    unsigned long long objs;
    unsigned long long bytes_in;
    unsigned long long bytes_out;
    unsigned long long t_cpu;
    unsigned long long s_cpu;
    unsigned long long clients;
    unsigned long long accepts;
    unsigned long long conns;
    unsigned long long s_bytes_in;
} stats;

/* swift register info for tsar */
struct mod_info swift_info[] = {
    {"   qps", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", DETAIL_BIT,  0,  STATS_NULL},
    {" r_hit", DETAIL_BIT,  0,  STATS_NULL},
    {" b_hit", DETAIL_BIT,  0,  STATS_NULL},
    {"  objs", DETAIL_BIT,  0,  STATS_NULL},
    {" in_bw", DETAIL_BIT,  0,  STATS_NULL},
    {"hit_bw", DETAIL_BIT,  0,  STATS_NULL},
    {"out_bw", DETAIL_BIT,  0,  STATS_NULL},
    {"   cpu", DETAIL_BIT,  0,  STATS_NULL},
    {"client", DETAIL_BIT,  0,  STATS_NULL},
    {"accept", DETAIL_BIT,  0,  STATS_NULL},
    {" conns", DETAIL_BIT,  0,  STATS_NULL},
    {"  live", DETAIL_BIT,  0,  STATS_NULL},
    {"  null", HIDE_BIT,  0,  STATS_NULL}
};

int
parse_swift_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_value(line, SWIFT_STORE[0], &stats.requests);
        read_swift_value(line, SWIFT_STORE[1], &stats.bytes_in);
        read_swift_value(line, SWIFT_STORE[2], &stats.bytes_out);
        read_swift_value(line, SWIFT_STORE[3], &stats.total_svc_time);
        read_swift_value(line, SWIFT_STORE[4], &stats.hits);
        read_swift_value(line, SWIFT_STORE[5], &stats.objs);
        read_swift_value(line, SWIFT_STORE[6], &stats.clients);
        read_swift_value(line, SWIFT_STORE[7], &stats.accepts);
        read_swift_value(line, SWIFT_STORE[8], &stats.conns);
        read_swift_value(line, SWIFT_STORE[9], &stats.s_bytes_in);
        /* Byte Hit Ratios:        5min: 96.6%, 60min: 96.6% */
        if (strstr(line, "Byte Hit Ratios") != NULL) {
            float a, b;
            sscanf(line, "        Byte Hit Ratios:        5min: %f%%, 60min: %f%%", &a, &b);
            if (a > 0)
                stats.b_hit = a * 1000;
            else
                stats.b_hit = 0;
        }
        /* UP Time:        247256.904 seconds */
        if (strstr(line, "UP Time") != NULL) {
            float a;
            sscanf(line, "        UP Time:        %f seconds", &a);
            stats.t_cpu = a * 1000;
        }
        /* CPU Time:       23487.042 seconds */
        if (strstr(line, "CPU Time") != NULL) {
            float a;
            sscanf(line, "        CPU Time:       %f seconds", &a);
            stats.s_cpu = a * 1000;
        }
        line = strtok(NULL, "\n");
    }
    return 0;
}

void
set_swift_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] -  pre_array[0]) / inter;

    } else {
        st_array[0] = 0;
    }
    /* rt */
    if (cur_array[0] > pre_array[0] && cur_array[1] > pre_array[1])
        st_array[1] = (cur_array[1] - pre_array[1]) / (cur_array[0] - pre_array[0]);
    else
        st_array[1] = 0;
    /* r_hit */
    if (cur_array[0] > pre_array[0] && cur_array[2] > pre_array[2])
        st_array[2] = 100 * (cur_array[2] - pre_array[2]) / (cur_array[0] - pre_array[0]);
    else
        st_array[2] = 0;
    /* b_hit */
    if (cur_array[3] > 0)
        st_array[3] = cur_array[3] * 1.0 / 1000;
    else
        st_array[3] = 0;
    /* objs */
    if (cur_array[4] > 0)
        st_array[4] = cur_array[4];
    else
        st_array[4] = 0;
    /* in_bw */
    if (cur_array[5] >= pre_array[5]) {
        st_array[5] = (cur_array[5] -  pre_array[5]) / inter;

    } else {
        st_array[5] = 0;
    }
    /* hit_bw = client_http.bytes_out - server_http.bytes_in */
    if (cur_array[6] >= pre_array[6] && cur_array[7] >= pre_array[7]
            && cur_array[7] - pre_array[7] >= cur_array[6] - pre_array[6]) {
        st_array[6] = ((cur_array[7] -  pre_array[7]) - (cur_array[6] -  pre_array[6])) / inter;

    } else {
        st_array[6] = 0;
    }
    /* out_bw */
    if (cur_array[7] >= pre_array[7]) {
        st_array[7] = (cur_array[7] -  pre_array[7]) / inter;

    } else {
        st_array[7] = 0;
    }
    /* cpu */
    if(cur_array[8] > pre_array[8] && cur_array[9] >= pre_array[9]) {
        st_array[8] = (cur_array[9] - pre_array[9]) * 100.0 / (cur_array[8] - pre_array[8]);

    } else {
        st_array[8] = 0;
    }
    /* clients */
    st_array[9] = cur_array[10];

    /* accepts */
    if (cur_array[11] >= pre_array[11]) {
        st_array[10] = (cur_array[11] -  pre_array[11]) / inter;
    } else {
        st_array[10] = 0;
    }

    /* conns */
    st_array[11] = cur_array[12];

    /* live */
    st_array[12] = cur_array[13];
}

int
read_swift_stat(char *cmd, parse_swift_info_func parse_func)
{
    char msg[LEN_1024];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/%s "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n",
            cmd);

    int len, conn = 0, bytesWritten, fsize = 0;

    if (my_swift_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    int flags;

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

    while ((len = myread_swift(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_func && parse_func(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

int
read_swift_health()
{
    char msg[LEN_512];
    char buf[1024*1024];
    sprintf(msg,
            "GET /status?SERVICE=swift HTTP/1.1\r\nConnection: close\r\n"
            "Host: cdn.hc.org\r\n\r\n");

    int len, conn = 0, bytesWritten, fsize = 0;
    int port = mgrport - 1;

    if (my_swift_net_connect(HOSTNAME, port, &conn, "tcp") != 0) {
        close(conn);
        return 0;
    }

    int flags;

    /* set socket fd noblock */
    if ((flags = fcntl(conn, F_GETFL, 0)) < 0) {
        close(conn);
        return 0;
    }

    if (fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(conn);
        return 0;
    }

    struct timeval timeout = {10, 0};

    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

    bytesWritten = mywrite_swift(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return 0;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return 0;
    }

    while ((len = myread_swift(conn, buf + fsize, sizeof(buf) - fsize) - 1) > 0) {
        fsize += len;
    }

    buf[fsize] = '\0';

    char *p = strstr(buf, "\r\n");
    if (p && memcmp(buf, "HTTP/1.1 200 OK", 15) == 0) {
        return 1;
    }

    return 0;
}

void
read_swift_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096];

    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 82;
    }
    while (read_swift_stat("info", parse_swift_info) < 0 && retry < RETRY_NUM) {
        retry++;
    }
    retry = 0;
    while (read_swift_stat("counters", parse_swift_info) < 0 && retry < RETRY_NUM) {
        retry++;
    }

    unsigned long long live = read_swift_health();

    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            stats.requests,
            stats.total_svc_time,
            stats.hits,
            stats.b_hit,
            stats.objs,
            stats.bytes_in,
            stats.s_bytes_in,
            stats.bytes_out,
            stats.t_cpu,
            stats.s_cpu,
            stats.clients,
            stats.accepts,
            stats.conns,
            live
             );
    buf[pos] = '\0';
    // fprintf(stderr, "buf: %s\n", buf);
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--swift", swift_usage, swift_info, 14, read_swift_stats, set_swift_record);
}

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

const static short int TS_RECORD_GET = 3;
const static int LINE_1024 = 1024;
const static int LINE_4096 = 4096;
const static char *RECORDS_NAME[]= {
    "proxy.process.http.incoming_requests",
    "proxy.process.http.total_client_connections",
    "proxy.node.http.user_agent_total_response_bytes",
    "proxy.node.http.user_agents_total_transactions_count",
    "proxy.process.http.total_transactions_time"
};
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";

/*
 * Structure for TS information
 */
struct stats_ts {
    unsigned long long qps;
    unsigned long long cons;
    unsigned long long mbps;
    unsigned long long uattc;
    unsigned long long uattt;
    unsigned long long rt;
    unsigned long long req_per_con;
};

static char *ts_usage = "    --ts                trafficserver client statistics";

static struct mod_info ts_info[] = {
    {"   qps", DETAIL_BIT, 0, STATS_NULL},
    {"  cons", DETAIL_BIT, 0, STATS_NULL},
    {"   Bps", DETAIL_BIT, 0, STATS_NULL},
    {" uattc", HIDE_BIT, 0, STATS_NULL},
    {" uattt", HIDE_BIT, 0, STATS_NULL},
    {"    rt", DETAIL_BIT, 0, STATS_NULL},
    {"   rpc", DETAIL_BIT, 0, STATS_NULL}
};

void
set_ts_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 7; ++i) {
        st_array[i] = 0;
    }
    for (i = 0; i < 5; ++i) {
        if (!cur_array[i])
        {
            cur_array[i] = pre_array[i];
        }
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        }
    }
    if (cur_array[0] >= pre_array[0] && cur_array[1] > pre_array[1]) {
        st_array[6] = st_array[0] / st_array[1];
    }
    if (cur_array[4] >= pre_array[4] && cur_array[3] > pre_array[1]) {
        st_array[5] = (st_array[4] / st_array[3]) / 1000000.0;
    }
}

void
read_ts_stats(struct module *mod)
{
    int                 pos;
    int                 fd = -1;
    char                buf[LINE_4096];
    struct stats_ts     st_ts;
    struct sockaddr_un  un;
    bzero(&st_ts, sizeof(st_ts));
    bzero(&un, sizeof(un));
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        goto done;
    }
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, sock_path);
    if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        goto done;
    }

    int          i, len;
    int          record_len = sizeof(RECORDS_NAME) / sizeof(RECORDS_NAME[0]);
    const char  *info;

    for ( i = 0; i < record_len; ++i) {
        info = RECORDS_NAME[i];
        char       write_buf[LINE_1024];
        long int   info_len = strlen(info);
        short int  command = TS_RECORD_GET;

        *((short int *)&write_buf[0]) = command;
        *((long int *)&write_buf[2]) = info_len;
        strcpy(write_buf + 6, info);
        len = 2 + 4 + strlen(info);
        if (write(fd, write_buf, len) != len) {
            close(fd);
            return;
        }

        int        read_len = read(fd, buf, LINE_1024);
        long       ret_val = 0;
        short int  ret_status = 0;
        short int  ret_type = 0;

        if (read_len != -1) {
            ret_status = *((short int *)&buf[0]);
            ret_type = *((short int *)&buf[6]);

        } else {
            close(fd);
            return;
        }
        if (0 == ret_status) {
            if (ret_type < 2) {
                ret_val= *((long int *)&buf[8]);

            } else if (2 == ret_type) {
                float ret_val_float = *((float *)&buf[8]);
                ret_val_float *= 100;
                ret_val = (unsigned long long)ret_val_float;
            } else {
                goto done;
            }
        }
        ((unsigned long long *)&st_ts)[i] = ret_val;
    }
done:
    if (-1 != fd) {
        close(fd);
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            st_ts.qps,
            st_ts.cons,
            st_ts.mbps,
            st_ts.uattc,
            st_ts.uattt,
            st_ts.rt,
            st_ts.req_per_con
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--ts", ts_usage, ts_info, 7, read_ts_stats, set_ts_record);
}

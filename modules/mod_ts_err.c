#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

/*
 * Structure for TS information
 */
struct stats_ts_err {
    unsigned long long miss_host;
    unsigned long long aborts;
    unsigned long long pre_accept_hangups;
    unsigned long long empty_hangups;
    unsigned long long early_hangups;
    unsigned long long con_fail;
    unsigned long long other;
    unsigned long long hangup;
};
/* command type */
const static short int TS_RECORD_GET = 3;
/* records */
const static int LINE_1024 = 1024;
const static int LINE_4096 = 4096;
const static char *RECORDS_NAME[]= {
    "proxy.process.http.missing_host_hdr",
    "proxy.process.http.transaction_counts.errors.aborts",
    "proxy.process.http.transaction_counts.errors.pre_accept_hangups",
    "proxy.process.http.transaction_counts.errors.empty_hangups",
    "proxy.process.http.transaction_counts.errors.early_hangups",
    "proxy.process.http.transaction_counts.errors.connect_failed",
    "proxy.process.http.transaction_counts.errors.other"//float
};
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";

static char *ts_err_usage = "    --ts_err            trafficserver error statistics";

static struct mod_info ts_err_info[] = {
    {"  host", DETAIL_BIT, 0, STATS_NULL},
    {" abort", DETAIL_BIT, 0, STATS_NULL},
    {"   p_h", HIDE_BIT, 0, STATS_NULL},
    {" emp_h", HIDE_BIT, 0, STATS_NULL},
    {" ear_h", HIDE_BIT, 0, STATS_NULL},
    {"  conn", DETAIL_BIT, 0, STATS_NULL},
    {" other", DETAIL_BIT, 0, STATS_NULL},
    {"hangup", SUMMARY_BIT, 0, STATS_NULL}
};

void
set_ts_err_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 8; ++i) {
        st_array[i] = 0;
    }
    for (i = 0; i < 6; ++i) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        }
    }
    if (cur_array[6] >= pre_array[6]) {
        st_array[6] = (cur_array[6] - pre_array[6]) * 1.0 / (inter * 100);
    }
    st_array[7] = st_array[2] + st_array[3] + st_array[4];
}

void
read_ts_err_stats(struct module *mod)
{
    int                  fd = -1;
    int                  pos;
    char                 buf[LINE_4096];
    struct sockaddr_un   un;
    struct stats_ts_err  st_ts;
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
        long int info_len = strlen(info);
        short int command = TS_RECORD_GET;
        char write_buf[LINE_1024];
        *((short int *)&write_buf[0]) = command;
        *((long int *)&write_buf[2]) = info_len;
        strcpy(write_buf + 6, info);
        len = 2 + 4 + strlen(info);
        if (write(fd, write_buf, len) != len) {
            close(fd);
            return;
        }

        short int ret_status = 0;
        short int ret_type = 0;
        long ret_val = 0;
        int read_len = read(fd, buf, LINE_1024);
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
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            st_ts.miss_host,
            st_ts.aborts,
            st_ts.pre_accept_hangups,
            st_ts.empty_hangups,
            st_ts.early_hangups,
            st_ts.con_fail,
            st_ts.other,
            st_ts.hangup
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--ts_err", ts_err_usage, ts_err_info, 8, read_ts_err_stats, set_ts_err_record);
}

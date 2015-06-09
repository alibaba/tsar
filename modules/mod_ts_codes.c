#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

/*
 *return value type
 *const static short int TS_REC_INT = 0;
 *const static short int TS_REC_COUNTER = 0;
 *const static short int TS_REC_FLOAT = 2;
 *const static short int TS_REC_STRING = 3;
 *command type
*/
const static short int TS_RECORD_GET = 3;
const static int LINE_1024 = 1024;
const static int LINE_4096 = 4096;
const static char *RECORDS_NAME[]= {
    "proxy.process.http.200_responses",
    "proxy.process.http.206_responses",
    "proxy.process.http.301_responses",
    "proxy.process.http.302_responses",
    "proxy.process.http.304_responses",
    "proxy.process.http.400_responses",
    "proxy.process.http.403_responses",
    "proxy.process.http.404_responses",
    "proxy.process.http.500_responses",
    "proxy.process.http.502_responses",
    "proxy.process.http.503_responses",
    "proxy.process.http.504_responses"
};
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";

/*
 * Structure for TS information
 */
struct stats_ts_codes {
    unsigned long long code200;
    unsigned long long code206;
    unsigned long long code301;
    unsigned long long code302;
    unsigned long long code304;
    unsigned long long code400;
    unsigned long long code403;
    unsigned long long code404;
    unsigned long long code500;
    unsigned long long code502;
    unsigned long long code503;
    unsigned long long code504;
};

static char *ts_codes_usage = "    --ts_codes          trafficserver statistics on http response codes";

static struct mod_info ts_code_info[] = {
    {"   200", DETAIL_BIT, 0, STATS_NULL},
    {"   206", DETAIL_BIT, 0, STATS_NULL},
    {"   301", DETAIL_BIT, 0, STATS_NULL},
    {"   302", DETAIL_BIT, 0, STATS_NULL},
    {"   304", DETAIL_BIT, 0, STATS_NULL},
    {"   400", DETAIL_BIT, 0, STATS_NULL},
    {"   403", DETAIL_BIT, 0, STATS_NULL},
    {"   404", DETAIL_BIT, 0, STATS_NULL},
    {"   500", DETAIL_BIT, 0, STATS_NULL},
    {"   502", DETAIL_BIT, 0, STATS_NULL},
    {"   503", DETAIL_BIT, 0, STATS_NULL},
    {"   504", DETAIL_BIT, 0, STATS_NULL},
};

void
set_ts_code_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 12; i++) {
        if (!cur_array[i])
        {
            cur_array[i] = pre_array[i];
        }
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        }
    }
}

void
read_ts_code_stats(struct module *mod)
{
    int                    pos;
    int                    fd = -1;
    char                   buf[LINE_4096];
    struct sockaddr_un     un;
    struct stats_ts_codes  st_ts;
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
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            st_ts.code200,
            st_ts.code206,
            st_ts.code301,
            st_ts.code302,
            st_ts.code304,
            st_ts.code400,
            st_ts.code403,
            st_ts.code404,
            st_ts.code500,
            st_ts.code502,
            st_ts.code503,
            st_ts.code504
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--ts_codes", ts_codes_usage, ts_code_info, 12, read_ts_code_stats, set_ts_code_record);
}

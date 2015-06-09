#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

/* * Structure for TS storage information
 */
struct stats_ts_storage {
    unsigned long long ram_used_space;
    unsigned long long disk_used_space;
    unsigned long long dirs_used;
    unsigned long long avg_obj_size;
};
const static short int TS_RECORD_GET = 3;
const static int LINE_1024 = 1024;
const static int LINE_4096 = 4096;
const static char *RECORDS_NAME[]= {
    "proxy.process.cache.ram_cache.bytes_used",
    "proxy.process.cache.bytes_used",
    "proxy.process.cache.direntries.used"
};
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";

static char *ts_storage_usage = "    --ts_storage        trafficserver storage statistics";

static struct mod_info ts_storage_info[] = {
    {"   ram", DETAIL_BIT, 0, STATS_NULL},
    {"  disk", DETAIL_BIT, 0, STATS_NULL},
    {"  objs", DETAIL_BIT, 0, STATS_NULL},
    {"  size", DETAIL_BIT, 0, STATS_NULL}
};

void
set_ts_storage_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    st_array[3] = 0;
    for (i = 0; i < 3; ++i) {
        st_array[i] = cur_array[i];
    }
    if (cur_array[2] != 0) {
        st_array[3] = st_array[1] / st_array[2];
    }
}

void
read_ts_storage_stats(struct module *mod)
{
    int                      pos;
    int                      fd = -1;
    char                     buf[LINE_4096];
    struct sockaddr_un       un;
    struct stats_ts_storage  st_ts;
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
    pos = sprintf(buf, "%lld,%lld,%lld,%lld",
            st_ts.ram_used_space,
            st_ts.disk_used_space,
            st_ts.dirs_used,
            st_ts.avg_obj_size
             );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--ts_storage", ts_storage_usage, ts_storage_info, 4, read_ts_storage_stats, set_ts_storage_record);
}

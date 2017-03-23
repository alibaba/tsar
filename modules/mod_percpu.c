#include "tsar.h"

#define STAT_PATH "/proc/stat"

static char *percpu_usage = "    --percpu            Per cpu share (user, system, interrupt, nice, & idle)";

struct stats_percpu {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_sys;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_steal;
    unsigned long long cpu_hardirq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_guest;
    char cpu_name[10];
};

#define STATS_PERCPU_SIZE (sizeof(struct stats_percpu))

static void
read_percpu_stats(struct module *mod)
{
    int                  pos = 0, cpus = 0;
    FILE                *fp;
    char                 line[LEN_1M];
    char                 buf[LEN_1M];
    struct stats_percpu  st_percpu;

    memset(buf, 0, LEN_1M);
    memset(&st_percpu, 0, STATS_PERCPU_SIZE);
    if ((fp = fopen(STAT_PATH, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_1M, fp) != NULL) {
        if (!strncmp(line, "cpu", 3)) {
            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    st_percpu.cpu_name,
                    &st_percpu.cpu_user,
                    &st_percpu.cpu_nice,
                    &st_percpu.cpu_sys,
                    &st_percpu.cpu_idle,
                    &st_percpu.cpu_iowait,
                    &st_percpu.cpu_hardirq,
                    &st_percpu.cpu_softirq,
                    &st_percpu.cpu_steal,
                    &st_percpu.cpu_guest);
            if (st_percpu.cpu_name[3] == '\0') //ignore cpu summary stat
                continue;

            pos += snprintf(buf + pos, LEN_1M - pos, "%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu" ITEM_SPLIT,
                    /* the store order is not same as read procedure */
                    st_percpu.cpu_name,
                    st_percpu.cpu_user,
                    st_percpu.cpu_sys,
                    st_percpu.cpu_iowait,
                    st_percpu.cpu_hardirq,
                    st_percpu.cpu_softirq,
                    st_percpu.cpu_idle,
                    st_percpu.cpu_nice,
                    st_percpu.cpu_steal,
                    st_percpu.cpu_guest);
            if (strlen(buf) == LEN_1M - 1) {
                fclose(fp);
                return;
            }
            cpus++;
        }
    }
    set_mod_record(mod, buf);
    fclose(fp);
    return;
}

static void
set_percpu_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int    i, j;
    U_64   pre_total, cur_total;
    pre_total = cur_total = 0;

    for (i = 0; i < mod->n_col; i++) {
        if(cur_array[i] < pre_array[i]){
            for(j = 0; j < 9; j++)
                st_array[j] = -1;
            return;
        }
        pre_total += pre_array[i];
        cur_total += cur_array[i];
    }

    /* no tick changes, or tick overflows */
    if (cur_total <= pre_total)
        return;
    /* set st record */
    for (i = 0; i < 9; i++) {
        /* st_array[5] is util, calculate it late */
        if((i != 5) && (cur_array[i] >= pre_array[i]))
            st_array[i] = (cur_array[i] - pre_array[i]) * 100.0 / (cur_total - pre_total);
    }

    /* util = user + sys + hirq + sirq + nice */
    st_array[5] = st_array[0] + st_array[1] + st_array[3] + st_array[4] + st_array[6];
}

static struct mod_info percpu_info[] = {
    {"  user", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"   sys", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  wait", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  hirq", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  sirq", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  util", SUMMARY_BIT,  MERGE_SUM,  STATS_NULL},
    {"  nice", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
    {" steal", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
    {" guest", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--percpu", percpu_usage, percpu_info, 9, read_percpu_stats, set_percpu_record);
}

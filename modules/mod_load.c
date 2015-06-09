#include "tsar.h"

#define LOAD_DETAIL_HDR(d)          \
    "  runq"d" plist"d"  min1"d     \
"  min5"d" min15"

#define LOAD_STORE_FMT(d)           \
    "%ld"d"%d"d"%d"d"%d"d"%d"

char *load_usage = "    --load              System Run Queue and load average";

/* Structure for queue and load statistics */
struct stats_load {
    unsigned long nr_running;
    unsigned int  load_avg_1;
    unsigned int  load_avg_5;
    unsigned int  load_avg_15;
    unsigned int  nr_threads;
};

#define STATS_LOAD_SIZE (sizeof(struct stats_load))

void
read_stat_load(struct module *mod)
{
    int     load_tmp[3];
    FILE   *fp;
    char    buf[LEN_4096];
    struct  stats_load st_load;
    memset(buf, 0, LEN_4096);
    memset(&st_load, 0, sizeof(struct stats_load));

    if ((fp = fopen(LOADAVG, "r")) == NULL) {
        return;
    }

    /* Read load averages and queue length */
    if (fscanf(fp, "%d.%d %d.%d %d.%d %ld/%d %*d\n",
            &load_tmp[0], &st_load.load_avg_1,
            &load_tmp[1], &st_load.load_avg_5,
            &load_tmp[2], &st_load.load_avg_15,
            &st_load.nr_running,
            &st_load.nr_threads) != 8)
    {
        fclose(fp);
        return;
    }
    st_load.load_avg_1  += load_tmp[0] * 100;
    st_load.load_avg_5  += load_tmp[1] * 100;
    st_load.load_avg_15 += load_tmp[2] * 100;

    if (st_load.nr_running) {
        /* Do not take current process into account */
        st_load.nr_running--;
    }

    int pos = sprintf(buf , "%u,%u,%u,%lu,%u",
            st_load.load_avg_1,
            st_load.load_avg_5,
            st_load.load_avg_15,
            st_load.nr_running,
            st_load.nr_threads);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    fclose(fp);
}

static void
set_load_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 3; i++) {
        st_array[i] = cur_array[i] / 100.0;
    }
    st_array[3] = cur_array[3];
    st_array[4] = cur_array[4];
}

static struct mod_info load_info[] = {
    {" load1", SUMMARY_BIT,  0,  STATS_NULL},
    {" load5", DETAIL_BIT,  0,  STATS_NULL},
    {"load15", DETAIL_BIT,  0,  STATS_NULL},
    {"  runq", DETAIL_BIT,  0,  STATS_NULL},
    {"  plit", DETAIL_BIT,  0,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--load", load_usage, load_info, 5, read_stat_load, set_load_record);
}


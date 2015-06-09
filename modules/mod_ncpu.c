#include "tsar.h"

/*
 * Structure for CPU infomation.
 */
struct stats_cpu {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_sys;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_steal;
    unsigned long long cpu_hardirq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_guest;
};

#define STATS_CPU_SIZE (sizeof(struct stats_cpu))

static char *cpu_usage = "    --ncpu               CPU share (user, system, interrupt, nice, & idle)";


static void
read_cpu_stats(struct module *mod)
{
    int               pos = 0;
    char              line[LEN_1M];
    char              buf[LEN_1M];
    char              cpuname[16];
    FILE             *fp;
    struct stats_cpu  st_cpu;

    memset(buf, 0, LEN_1M);
    memset(&st_cpu, 0, sizeof(struct stats_cpu));
    if ((fp = fopen(STAT, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_1M, fp) != NULL) {
        if (!strncmp(line, "cpu", 3)) {
            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line, "%s", cpuname);
            if(strcmp(cpuname, "cpu") == 0)
                continue;
            sscanf(line + strlen(cpuname), "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &st_cpu.cpu_user,
                    &st_cpu.cpu_nice,
                    &st_cpu.cpu_sys,
                    &st_cpu.cpu_idle,
                    &st_cpu.cpu_iowait,
                    &st_cpu.cpu_hardirq,
                    &st_cpu.cpu_softirq,
                    &st_cpu.cpu_steal,
                    &st_cpu.cpu_guest);

            pos += snprintf(buf + pos, LEN_1M - pos, "%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu" ITEM_SPLIT,
                    /* the store order is not same as read procedure */
                    cpuname,
                    st_cpu.cpu_user,
                    st_cpu.cpu_sys,
                    st_cpu.cpu_iowait,
                    st_cpu.cpu_hardirq,
                    st_cpu.cpu_softirq,
                    st_cpu.cpu_idle,
                    st_cpu.cpu_nice,
                    st_cpu.cpu_steal,
		    st_cpu.cpu_guest);
	    if (strlen(buf) == LEN_1M - 1) {
                fclose(fp);
                return;
	    }
        }
    }
    set_mod_record(mod, buf);
    fclose(fp);
    return;
}

static void
set_cpu_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int    i;
    U_64   pre_total, cur_total;
    pre_total = cur_total = 0;

    for (i = 0; i < mod->n_col; i++) {
        pre_total += pre_array[i];
        cur_total += cur_array[i];
    }
    /* set st record */
    for (i = 0; i <= 4; i++) {
        if(cur_array[i] >= pre_array[i])
            st_array[i] = (cur_array[i] - pre_array[i]) * 100.0 / (cur_total - pre_total);
    }
    if(cur_array[5] >= pre_array[5])
        st_array[5] = 100.0 - (cur_array[5] - pre_array[5]) * 100.0 / (cur_total - pre_total);
}

static struct mod_info cpu_info[] = {
    {"  user", DETAIL_BIT,  0,  STATS_NULL},
    {"   sys", DETAIL_BIT,  0,  STATS_NULL},
    {"  wait", DETAIL_BIT,  0,  STATS_NULL},
    {"  hirq", DETAIL_BIT,  0,  STATS_NULL},
    {"  sirq", DETAIL_BIT,  0,  STATS_NULL},
    {"  util", SUMMARY_BIT,  0,  STATS_NULL},
    {"  nice", HIDE_BIT,  0,  STATS_NULL},
    {" steal", HIDE_BIT,  0,  STATS_NULL},
    {" guest", HIDE_BIT,  0,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--ncpu", cpu_usage, cpu_info, 9, read_cpu_stats, set_cpu_record);
}

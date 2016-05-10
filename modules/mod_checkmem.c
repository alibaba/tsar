#include "tsar.h"

/*
 * Structure for Proc infomation.
 */
typedef struct {
    char               name[20];
    char               master_pid[100];
    int                rate;
    unsigned long long pid;
    unsigned long long mem;
    unsigned long long aver_mem;
} proc_mem_t;

#define PID_STATUS      "/proc/%u/status"
#define SWIFT_PROCESS "/sbin/pidof swift"
#define PCMD "/usr/bin/pgrep -P %s"

#define NGINX_PROCESS_PATH "/home/admin/tengine/logs/t-coresystem-tengine-cdn.pid"
#define LIVE_PROCESS_PATH "/home/admin/live/logs/t-coresystem-tengine-live.pid"
#define PSCMD "ps -ef | grep %s | grep -v grep | awk '{print $2}'"
#define STATS_MEM_SIZE (sizeof(struct stats_mem))

static char *checkmem_usage = "    --checkmem               check process rss mem info (nginx nginx_live & swift)";

static void
read_mem_stat_info(proc_mem_t *stat_mem)
{
    int     nb = 0, i = 0, pid[100];
    char    filename[100], line[256];
    FILE    *fp;
    char    pcmd[100];
    char    spid[1000];
    char    *delch = " ", *cmd = NULL;
    if (!strncmp(stat_mem->name, "nginx", sizeof("nginx"))) {
        sprintf(pcmd, PCMD, stat_mem->master_pid);
        cmd = pcmd;
        delch = "\n";
    } else if (!strncmp(stat_mem->name, "live", sizeof("live"))) {
        sprintf(pcmd, PCMD, stat_mem->master_pid);
        cmd = pcmd;
        delch = "\n";
    } else if (!strncmp(stat_mem->name, "swift", sizeof("siwft"))) {
        cmd = SWIFT_PROCESS;
    } else {
        return ;
    }
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return;
    }

    if(fgets(spid, 1000, fp) ==NULL) {
        pclose(fp);
        return;
    }
    pclose(fp);
    /* split pidof into array pid */
    char *p;
    p = strtok(spid, delch);
    while (p) {
        pid[nb] = atoi(p);
        if (pid[nb++] <= 0) {
            return;
        }
        if (nb >= 100) {
            return;
        }
        p = strtok(NULL, " ");
    }
    /* get all pid's info */
    stat_mem->mem = 0;
    for (i = 0; i < nb; i++) {
        unsigned long long data;
        /* get cpu&mem info from /proc/pid/status */
        sprintf(filename, PID_STATUS, pid[i]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        while (fgets(line, 256, fp) != NULL) {
            if (!strncmp(line, "VmRSS:", 6)) {
                sscanf(line + 6, "%llu", &data);
                stat_mem->aver_mem += data * 1024;
                if (data > stat_mem->mem) {
                    stat_mem->mem = data * 1024;
                    stat_mem->pid = pid[i];
                }
            }
        }
        fclose(fp);
    }
    if (nb) {
        stat_mem->aver_mem = stat_mem->aver_mem / nb;
        stat_mem->rate = (unsigned long long)((stat_mem->mem * 100 )/ stat_mem->aver_mem);
    }
}

static void initializes(proc_mem_t *stats_mem, int *nitems)
{
    char pscmd[100];
    FILE *fp;
    if (access(NGINX_PROCESS_PATH, R_OK) == 0) {
        strncpy(stats_mem[(*nitems)].name, "nginx", sizeof("nginx"));
        if ((fp = fopen(NGINX_PROCESS_PATH, "r"))) {
            fgets(stats_mem[(*nitems)++].master_pid, 256, fp);
        }
    } else {
       sprintf(pscmd, PSCMD, "/home/admin/tengine/bin/t-coresystem-tengine-cdn");
       fp = popen(pscmd, "r");
       if (fp != NULL) {
           if(fgets(stats_mem[(*nitems)].master_pid, 100, fp)) {
               strncpy(stats_mem[(*nitems)++].name, "nginx", sizeof("nginx"));
           }
       }
    }

    if (access(LIVE_PROCESS_PATH, R_OK) == 0) {
        strncpy(stats_mem[(*nitems)].name, "live", sizeof("live"));
        if ((fp = fopen(LIVE_PROCESS_PATH, "r"))) {
            fgets(stats_mem[(*nitems)++].master_pid, 256, fp);
        }
    } else {
        sprintf(pscmd, PSCMD, "/home/admin/live/bin/t-coresystem-tengine-live");
        fp = popen(pscmd, "r");
        if (fp != NULL) {
           if(fgets(stats_mem[(*nitems)].master_pid, 100, fp)) {
               strncpy(stats_mem[(*nitems)++].name, "live", sizeof("live"));
           }
        }
    }

    strncpy(stats_mem[(*nitems)++].name, "swift", sizeof("swift"));
}

static void
read_mem_stats(struct module *mod, char *paramter)
{
    int     i = 0;
    int     pos = 0;
    int     nitems = 0;
    char    buf[LEN_4096];

    proc_mem_t stats_mem[30];
    memset(&stats_mem, 0, sizeof(proc_mem_t) * 30);
    initializes(stats_mem, &nitems);
    for (i = 0; i < nitems; i++) {
        read_mem_stat_info(&stats_mem[i]);
    }
    /* store data to tsar */
    for (i = 0; i < nitems; i++) {
        pos += snprintf(buf + pos, LEN_1M - pos, "%s=%llu,%llu,%llu,%d" ITEM_SPLIT,
                        stats_mem[i].name,
                        stats_mem[i].pid,
                        stats_mem[i].aver_mem,
                        stats_mem[i].mem,
                        stats_mem[i].rate);
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

static void
set_mem_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[0];
    st_array[1] = cur_array[1];
    st_array[2] = cur_array[2];
    st_array[3] = cur_array[3] * 1.0 / 100;
}

static struct mod_info mem_info[] = {
    {"   pid", DETAIL_BIT_LLU,  MERGE_NULL,  STATS_NULL},
    {"  aver", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {"   max", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {"  rate", DETAIL_BIT,  MERGE_NULL,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--checkmem", checkmem_usage, mem_info, 4, read_mem_stats, set_mem_record);
}

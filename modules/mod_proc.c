#include "tsar.h"

/*
 * Structure for Proc infomation.
 */
struct stats_proc {
    unsigned long long user_cpu;
    unsigned long long sys_cpu;
    unsigned long long total_cpu;
    unsigned long long mem;
    unsigned long long total_mem;
    unsigned long long read_bytes;
    unsigned long long write_bytes;
};

#define PID_IO          "/proc/%u/io"
#define PID_STAT        "/proc/%u/stat"
#define PID_STATUS      "/proc/%u/status"
#define STATS_PROC_SIZE (sizeof(struct stats_proc))

static char *proc_usage = "    --proc               PROC info (mem cpu usage & io/fd info)";


static void
read_proc_stats(struct module *mod, char *parameter)
{
    int     nb = 0, pid[16];
    char    buf[LEN_4096];
    char    filename[128], line[256];
    FILE   *fp;
    struct  stats_proc st_proc;

    memset(&st_proc, 0, sizeof(struct stats_proc));

    /* get pid by proc name */
    if (strlen(parameter) > 20) {
        return;
    }
    char cmd[32] = "/sbin/pidof ";
    strncat(cmd, parameter, sizeof(cmd) - strlen(cmd) -1);
    char  spid[256];
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return;
    }
    if(fscanf(fp, "%s", spid) == EOF) {
        pclose(fp);
        return;
    }
    pclose(fp);
    /* split pidof into array pid */
    char *p;
    p = strtok(spid, " ");
    while (p) {
        pid[nb] = atoi(p);
        if (pid[nb++] <= 0) {
            return;
        }
        if (nb >= 16) {
            return;
        }
        p = strtok(NULL, " ");
    }
    /* get all pid's info */
    while (--nb >= 0) {
        unsigned long long data;
        /* read values from /proc/pid/stat */
        sprintf(filename, PID_STAT, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        unsigned long long cpudata[4];
        if (fgets(line, 256, fp) == NULL) {
            fclose(fp);
            return;
        }

        p = strstr(line, ")");
        if (sscanf(p, "%*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %llu %llu",
                    &cpudata[0], &cpudata[1], &cpudata[2], &cpudata[3]) == EOF) {
            fclose(fp);
            return;
        }
        st_proc.user_cpu += cpudata[0];
        st_proc.user_cpu += cpudata[2];
        st_proc.sys_cpu += cpudata[1];
        st_proc.sys_cpu += cpudata[3];
        fclose(fp);
        /* get cpu&mem info from /proc/pid/status */
        sprintf(filename, PID_STATUS, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        while (fgets(line, 256, fp) != NULL) {
            if (!strncmp(line, "VmRSS:", 6)) {
                sscanf(line + 6, "%llu", &data);
                st_proc.mem += data * 1024;
            }
        }
        fclose(fp);
        /* get io info from /proc/pid/io */
        sprintf(filename, PID_IO, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        while (fgets(line, 256, fp) != NULL) {
            if (!strncmp(line, "read_bytes:", 11)) {
                sscanf(line + 11, "%llu", &data);
                st_proc.read_bytes += data;
            }
            else if (!strncmp(line, "write_bytes:", 12)) {
                sscanf(line + 12, "%llu", &data);
                st_proc.write_bytes += data;
            }
        }
        fclose(fp);
    }

    /* read+calc cpu total time from /proc/stat */
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        fclose(fp);
        return;
    }
    unsigned long long cpu_time[10];
    bzero(cpu_time, sizeof(cpu_time));
    if (fscanf(fp, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                &cpu_time[8], &cpu_time[9]) == EOF) {
        fclose(fp);
        return;
    }
    fclose(fp);
    int i;
    for(i=0; i < 10;i++) {
        st_proc.total_cpu += cpu_time[i];
    }
    /* read total mem from /proc/meminfo */
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        fclose(fp);
        return;
    }
    if (fscanf(fp, "MemTotal:      %llu kB", &st_proc.total_mem) == EOF) {
        fclose(fp);
        return;
    }
    st_proc.total_mem *= 1024;
    fclose(fp);
    /* store data to tsar */
    int pos = 0;
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld",
            st_proc.user_cpu,
            st_proc.sys_cpu,
            st_proc.total_cpu,
            st_proc.total_mem,
            st_proc.mem,
            st_proc.read_bytes,
            st_proc.write_bytes
            );
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

static void
set_proc_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[2] > pre_array[2]) {
        st_array[2] = cur_array[2] - pre_array[2];
    } else {
        st_array[2] = 0;
        return;
    }
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] - pre_array[0]) * 100.0 / st_array[2];
    } else {
        st_array[0] = 0;
    }
    if (cur_array[1] >= pre_array[1]) {
        st_array[1] = (cur_array[1] - pre_array[1]) * 100.0 / st_array[2];
    } else {
        st_array[1] = 0;
    }
    st_array[3] = cur_array[4] * 100.0 / cur_array[3];
    st_array[4] = cur_array[4];
    if (cur_array[5] >= pre_array[5]) {
        st_array[5] = (cur_array[5] - pre_array[5]) * 1.0 / inter;
    } else {
        st_array[5] = 0;
    }
    if (cur_array[6] >= pre_array[6]) {
        st_array[6] = (cur_array[6] - pre_array[6]) * 1.0 / inter;
    } else {
        st_array[5] = 0;
    }
}

static struct mod_info proc_info[] = {
    {"  user", SUMMARY_BIT,  0,  STATS_NULL},
    {"   sys", SUMMARY_BIT,  0,  STATS_NULL},
    {" total", HIDE_BIT,  0,  STATS_NULL},
    {"   mem", SUMMARY_BIT,  0,  STATS_NULL},
    {"   RSS", DETAIL_BIT,  0,  STATS_NULL},
    {"  read", DETAIL_BIT,  0,  STATS_NULL},
    {" write", DETAIL_BIT,  0,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--proc", proc_usage, proc_info, 7, read_proc_stats, set_proc_record);
}

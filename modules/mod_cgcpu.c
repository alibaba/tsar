#include <dirent.h>
#include <assert.h>
#include "tsar.h"

static char *cgcpu_usage = "    --cgcpu            cgroup cpu statistics";

#define MAX_GROUP 64
#define MAX_TASK 1024
#define MAX_NAME_LENGTH 128
#define CGCPU_PATH "/cgroup/cpu"

#define ISDOT(a)        (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

unsigned int n_group;

struct task_info {
    int pid;
} tasks[MAX_TASK];

struct sched_info {
    char    name[512];
    char    none[4];
    double  time;
};

struct cgcpu_group_info {
    char    group_name [MAX_NAME_LENGTH];
    double  sum_exec_runtime;     /*sum of exec runtime counters*/
};
struct cgcpu_group_info cgcpu_groups[MAX_GROUP];
#define CGCPU_GROUP_SIZE (sizeof(struct cgcpu_group_info))

static struct mod_info cgcpu_info[] = {
    {"  util", DETAIL_BIT,  0,  STATS_SUB},
};


static void
set_cgcpu_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = ( cur_array[i] - pre_array[i] ) / (10000.0 * inter);
            if (st_array[i] > 100.0)
                st_array[i] = 100.0;
        }
    }
}

void
print_cgcpu_stats(struct module *mod)
{
    int    pos = 0, i=0;
    char   buf[LEN_1M];
    /*set n group's data to buf*/
    for (i = 0; i < n_group; i++) {
        pos += snprintf(buf + pos, LEN_1M, "%s=%llu", cgcpu_groups[i].group_name, (
                        unsigned long long int)(cgcpu_groups[i].sum_exec_runtime * 1000));
        if (pos >= LEN_1M) {
            break;
        }
        pos += snprintf(buf + pos, LEN_1M, ITEM_SPLIT);
        if (pos >= LEN_1M) {
            break;
        }
    }
    /*notice tsar to store my mult item data*/
    set_mod_record(mod, buf);
}

void
read_cgcpu_stats(struct module *mod)
{
    DIR            *dir;
    int             n_task = 0, i;
    FILE           *taskfd, *schedfd;
    char            path[128], buffer[128];
    const char     *scan_fmt = NULL;
    struct dirent  *ent;          /* dirent handle */

    n_group = 0;

    memset(cgcpu_groups, 0, CGCPU_GROUP_SIZE * MAX_GROUP);
    if ((dir = opendir(CGCPU_PATH)) == NULL) {
        return;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_type == DT_DIR && !ISDOT(ent->d_name)) {
            n_task = 0;
            memcpy(&cgcpu_groups[n_group].group_name, ent->d_name, strlen(ent->d_name) + 1);

            snprintf(path, 128, "%s/%s/tasks", CGCPU_PATH, ent->d_name);
            if ((taskfd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            /* for each task */
            while (fgets(buffer, sizeof(buffer), taskfd)) {
                struct task_info curr;

                if (sscanf(buffer, "%d", &curr.pid) == 1) {
                    tasks[n_task].pid = curr.pid;
                    n_task ++;
                }
            }
            n_task --;
            if (fclose(taskfd) < 0) {
                return;
            }

            assert(n_task + 1 <= MAX_TASK);

            /* read sum_exe_time of each task and add up */
            for (i = 0; i <= n_task; i++) {
                snprintf(path, 128, "/proc/%d/sched", tasks[i].pid);
                if ((schedfd = fopen(path, "r")) == NULL) {
                    closedir(dir);
                    return;
                }
                scan_fmt = "%s %s %lf";
                while (fgets(buffer, sizeof(buffer), schedfd)) {
                    char *title="se.sum_exec_runtime";
                    struct sched_info cur;

                    if (sscanf(buffer, scan_fmt, &cur.name, &cur.none, &cur.time) < 0){
                        return;
                    }
                    if (memcmp(cur.name, title, strlen(title)) == 0) {
                        cgcpu_groups[n_group].sum_exec_runtime += cur.time;
                        break;
                    }
                }
                if (fclose(schedfd) < 0) {
                    return;
                }
            }
            n_group ++;
        }
    }

    closedir(dir);
    print_cgcpu_stats(mod);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--cgcpu", cgcpu_usage, cgcpu_info, 1, read_cgcpu_stats, set_cgcpu_record);
}

#include <dirent.h>
#include <assert.h>
#include "tsar.h"

static char *cgmem_usage = "    --cgmem            cgroup memory statistics";

#define MAX_GROUP 64
#define MAX_NAME_LENGTH 128
#define CGMEM_PATH "/cgroup/memory"
#define CGMEM_UNIT ( 1024 )

#define ISDOT(a)        (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

unsigned int n_group;

struct cgmem_group_info {
    char          group_name [MAX_NAME_LENGTH];
    unsigned long cache;
    unsigned long rss;
    unsigned long swap;
    unsigned long inanon;
    unsigned long acanon;
    unsigned long infile;
    unsigned long acfile;
};
struct cgmem_group_info cgmem_groups[MAX_GROUP];
#define CGMEM_GROUP_SIZE (sizeof(struct cgmem_group_info))

static struct mod_info cgmem_info[] = {
    {"   mem", DETAIL_BIT,  0,  STATS_NULL},
    {"  swap", DETAIL_BIT,  0,  STATS_NULL},
    {"inanon", DETAIL_BIT,  0,  STATS_NULL},
    {"acanon", DETAIL_BIT,  0,  STATS_NULL},
    {"infile", DETAIL_BIT,  0,  STATS_NULL},
    {"acfile", DETAIL_BIT,  0,  STATS_NULL},
};


static void
set_cgmem_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        st_array[i] = cur_array[i] / CGMEM_UNIT;
    }
}

void
print_cgmem_stats(struct module *mod)
{
    int    pos = 0, i = 0;
    char   buf[LEN_4096];

    /*set n group's data to buf*/
    for (i = 0; i < n_group; i++) {
        pos += snprintf(buf + pos, LEN_4096, "%s=%lu,%lu,%lu,%lu,%lu,%lu",
                cgmem_groups[i].group_name,
                cgmem_groups[i].cache + cgmem_groups[i].rss,
                cgmem_groups[i].swap,
                cgmem_groups[i].inanon,
                cgmem_groups[i].acanon,
                cgmem_groups[i].infile,
                cgmem_groups[i].acfile);
        if (pos >= LEN_4096) {
            break;
        }
        pos += snprintf(buf + pos, LEN_4096, ITEM_SPLIT);
        if (pos >= LEN_4096) {
            break;
        }
    }
    /*notice tsar to store my mult item data*/
    set_mod_record(mod, buf);
}

void
read_cgmem_stats(struct module *mod)
{
    DIR             *dir;
    char            path[128];
    char            line[LEN_128];
    FILE           *memfd;
    struct dirent  *ent;          /* dirent handle */

    n_group = 0;

    memset(cgmem_groups, 0, CGMEM_GROUP_SIZE * MAX_GROUP);
    if ((dir = opendir(CGMEM_PATH)) == NULL) {
        return;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_type == DT_DIR && !ISDOT(ent->d_name)) {  //for each group
            memcpy(&cgmem_groups[n_group].group_name, ent->d_name, strlen(ent->d_name) + 1);
            snprintf(path, 128, "%s/%s/memory.stat", CGMEM_PATH, ent->d_name);
            if ((memfd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }

            while (fgets(line, 128, memfd) != NULL) {
                if (!strncmp(line, "cache", 5)) {
                    sscanf(line + 5, "%lu", &cgmem_groups[n_group].cache);

                } else if (!strncmp(line, "rss", 3)) {
                    sscanf(line + 3, "%lu", &cgmem_groups[n_group].rss);

                } else if (!strncmp(line, "swap", 4)) {
                    sscanf(line + 4, "%lu", &cgmem_groups[n_group].swap);

                } else if (!strncmp(line, "inactive_anon", 13)) {
                    sscanf(line + 13, "%lu", &cgmem_groups[n_group].inanon);

                } else if (!strncmp(line, "active_anon", 11)) {
                    sscanf(line + 11, "%lu", &cgmem_groups[n_group].acanon);

                } else if (!strncmp(line, "inactive_file", 13)) {
                    sscanf(line + 13, "%lu", &cgmem_groups[n_group].infile);

                } else if (!strncmp(line, "active_file", 11)) {
                    sscanf(line + 11, "%lu", &cgmem_groups[n_group].acfile);
                }
            }

            if (fclose(memfd) < 0) {
                return;
            }
            n_group ++;
        }
    }

    if (closedir(dir) < 0) {
        return;
    }
    print_cgmem_stats(mod);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--cgmem", cgmem_usage, cgmem_info, 6, read_cgmem_stats, set_cgmem_record);
}

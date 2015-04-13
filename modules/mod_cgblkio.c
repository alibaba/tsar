#include <dirent.h>
#include <assert.h>
#include "tsar.h"

static char *cgblkio_usage = "    --cgblkio            cgroup blkio statistics";

#define MAX_GROUP 64
#define MAX_NAME_LENGTH 128
#define CGBLKIO_PATH "/cgroup/blkio"
#define SECTOR_SIZE 512
#define MAX_DISKNAME 32

#define ISDOT(a)        (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

unsigned int n_group;

struct cgblkio_group_info {
    char               group_name[MAX_NAME_LENGTH];
    unsigned long long rd_merges;
    unsigned long long wr_merges;
    unsigned long long rd_ios;
    unsigned long long wr_ios;
    unsigned long long rd_secs;
    unsigned long long wr_secs;
    unsigned long long qusize;
    unsigned long long wait;
    unsigned long long svctm;
};

#define CGBLKIO_GROUP_SIZE (sizeof(struct cgblkio_group_info))
struct cgblkio_group_info blkio_groups[MAX_GROUP];

struct blkio_info {
    char                disk[MAX_DISKNAME];
    char                type[8];
    unsigned long long  num;
};

static struct mod_info cgblkio_info[] = {
    {" rrqms", DETAIL_BIT,  0,  STATS_NULL},
    {" wrqms", DETAIL_BIT,  0,  STATS_NULL},
    {"    rs", DETAIL_BIT,  0,  STATS_NULL},
    {"    ws", DETAIL_BIT,  0,  STATS_NULL},
    {" rsecs", DETAIL_BIT,  0,  STATS_NULL},
    {" wsecs", DETAIL_BIT,  0,  STATS_NULL},
    {"rqsize", DETAIL_BIT,  0,  STATS_NULL},
    {"qusize", DETAIL_BIT,  0,  STATS_NULL},
    {" await", DETAIL_BIT,  0,  STATS_NULL},
    {" svctm", DETAIL_BIT,  0,  STATS_NULL},
    {"  util", DETAIL_BIT,  0,  STATS_NULL},
};


static void
set_cgblkio_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    for(i = 0; i < 9; i++){
        if(cur_array[i] < pre_array[i]){
            pre_array[i] = cur_array[i];
        }
    }

    unsigned long long rd_merges = cur_array[0] - pre_array[0];
    unsigned long long wr_merges = cur_array[1] - pre_array[1];
    unsigned long long rd_ios = cur_array[2] - pre_array[2];
    unsigned long long wr_ios = cur_array[3] - pre_array[3];
    unsigned long long rd_secs = cur_array[4] - pre_array[4];
    unsigned long long wr_secs = cur_array[5] - pre_array[5];
    unsigned long long qusize = cur_array[6] - pre_array[6];
    unsigned long long svctm = cur_array[8] - pre_array[8];
    unsigned long long await = cur_array[7] - pre_array[7] + svctm;
    unsigned long long n_ios = rd_ios + wr_ios;
    unsigned long long n_kbytes = (rd_secs + wr_secs) / 2;

    st_array[0] = rd_merges / (inter * 1.0);
    st_array[1] = wr_merges / (inter * 1.0);
    st_array[2] = rd_ios / (inter * 1.0);
    st_array[3] = wr_ios / (inter * 1.0);
    st_array[4] = rd_secs / (inter * 1.0);
    st_array[5] = wr_secs / (inter * 1.0);
    st_array[6] = n_ios ? n_kbytes / n_ios : 0.0;
    st_array[7] = qusize / (inter * 1.0);
    st_array[8] = n_ios ? await / n_ios : 0.0;
    st_array[9] = n_ios ? svctm / n_ios : 0.0;
    st_array[10] = svctm / (inter * 1.0);

    if (st_array[10] > 100.0)
        st_array[10] = 100.0;

}

void
print_cgblkio_stats(struct module *mod)
{
    int    pos = 0, i = 0;
    char   buf[LEN_1M];
    /*set n group's data to buf*/
    for(i = 0; i < n_group; i++){
        pos += snprintf(buf + pos, LEN_1M, "%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                blkio_groups[i].group_name,
                blkio_groups[i].rd_merges,
                blkio_groups[i].wr_merges,
                blkio_groups[i].rd_ios,
                blkio_groups[i].wr_ios,
                blkio_groups[i].rd_secs,
                blkio_groups[i].wr_secs,
                blkio_groups[i].qusize,
                blkio_groups[i].wait,
                blkio_groups[i].svctm);
        if(pos >= LEN_1M)
            break;
        pos += snprintf(buf + pos, LEN_1M, ITEM_SPLIT);
        if(pos >= LEN_1M)
            break;
    }
    /*notice tsar to store my mult item data*/
    set_mod_record(mod, buf);
}

void
read_cgblkio_stats(struct module *mod)
{
    DIR               *dir;
    char               path[128], buffer[128];
    FILE              *iofd;
    struct dirent     *ent;          /* dirent handle */
    struct blkio_info  curr;

    n_group = 0;

    memset(blkio_groups, 0, CGBLKIO_GROUP_SIZE * MAX_GROUP);
    if ((dir = opendir(CGBLKIO_PATH)) == NULL) {
        return;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_type == DT_DIR && !ISDOT(ent->d_name)) {
            memcpy(&blkio_groups[n_group].group_name, ent->d_name, strlen(ent->d_name) + 1);

            snprintf(path, 128, "%s/%s/blkio.io_merged", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].rd_merges += curr.num;
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].wr_merges += curr.num;
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            snprintf(path, 128, "%s/%s/blkio.io_serviced", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].rd_ios += curr.num;
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].wr_ios += curr.num;
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            snprintf(path, 128, "%s/%s/blkio.io_service_bytes", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].rd_secs += curr.num / SECTOR_SIZE;
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].wr_secs += curr.num / SECTOR_SIZE;
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            snprintf(path, 128, "%s/%s/blkio.io_queued", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].qusize += curr.num;
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].qusize += curr.num;
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            snprintf(path, 128, "%s/%s/blkio.io_service_time", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].svctm += (unsigned long long)(curr.num / 1000000); //in ms
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].svctm += (unsigned long long )(curr.num / 1000000);
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            snprintf(path, 128, "%s/%s/blkio.io_wait_time", CGBLKIO_PATH, ent->d_name);
            if ((iofd = fopen(path, "r")) == NULL) {
                closedir(dir);
                return;
            }
            while (fgets(buffer, 128, iofd) != NULL) {
                if (sscanf(buffer, "%s %s %llu", curr.disk, curr.type, &curr.num) == 3) {
                    if (!strncmp(curr.type, "Read", 4))
                        blkio_groups[n_group].wait += (unsigned long long)(curr.num / 1000000);
                    if (!strncmp(curr.type, "Write", 5))
                        blkio_groups[n_group].wait += (unsigned long long)(curr.num / 1000000);
                }
            }
            if (fclose(iofd) < 0) {
                return;
            }

            n_group ++;
        }
    }

    closedir(dir);
    print_cgblkio_stats(mod);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--cgblkio", cgblkio_usage, cgblkio_info, 11, read_cgblkio_stats, set_cgblkio_record);
}

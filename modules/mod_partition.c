#include <mntent.h>
#include <sys/statfs.h>
#include "tsar.h"

#define TRUE 1
#define FALSE 0

char *partition_usage = "    --partition         Disk and partition usage";


#define MAXPART 32

struct stats_partition {
    int bsize;                           /* block size*/
    unsigned long long blocks;           /* total blocks*/
    unsigned long long bfree;            /* free for non root user*/
    unsigned long long bavail;           /* avail for root*/
    unsigned long long itotal;
    unsigned long long ifree;
};
#define STATS_PARTITION_SIZE (sizeof(struct stats_partition))

int
__read_partition_stat(char *fsname, struct stats_partition *sp)
{
    struct statfs fsbuf;
    if (!statfs(fsname, &fsbuf)) {
        sp->bsize = fsbuf.f_bsize;
        sp->blocks = fsbuf.f_blocks;
        sp->bfree = fsbuf.f_bfree;
        sp->bavail = fsbuf.f_bavail;
        sp->itotal = fsbuf.f_files;
        sp->ifree = fsbuf.f_ffree;
    }
    return 0;
}

int
store_single_partition(char *buf, char *mntpath, struct stats_partition *sp, int size)
{
    int                 len = 0;
    int                 k = 1;
    if (sp->bsize % 1024 != 0) {
	    return len;
    } else {
	    k = sp->bsize / 1024;
    }
    len += snprintf(buf + len, size, "%s=%d,%lld,%lld,%lld,%lld,%lld" ITEM_SPLIT,
            mntpath,
            sp->bsize / k,
            sp->bfree * k,
            sp->blocks * k,
            sp->bavail * k,
            sp->ifree,
            sp->itotal);
    return len;
}


void
read_partition_stat(struct module *mod)
{
    int                     part_nr, pos = 0;
    char                    buf[LEN_1M];
    FILE                   *mntfile;
    struct mntent          *mnt = NULL;
    struct stats_partition  temp;

    memset(buf, 0, LEN_1M);
    memset(&temp, 0, sizeof(temp));

    /* part_nr = count_partition_nr(NULL); */

    mntfile = setmntent("/etc/mtab", "r");

    /* init part_nr */
    part_nr = 0;
    /* traverse the mount table */
    while ((mnt = getmntent(mntfile)) != NULL) {
        /* only recore block filesystems */
        if (! strncmp(mnt->mnt_fsname, "/", 1)) {
            /* we only read MAXPART partition */
            if (part_nr >= MAXPART) break;
            /* read each partition infomation */
            __read_partition_stat(mnt->mnt_dir, &temp);

            /* print log to the buffer */
	    pos += store_single_partition(buf + pos, mnt->mnt_dir, &temp, LEN_1M - pos);
            if (strlen(buf) == LEN_1M - 1) {
                return;
            }
            /* successful read */
            part_nr++;
            /* move the pointer to the next structure */
        }
    }
    endmntent(mntfile);
    set_mod_record(mod, buf);
    return;
}

static void
set_part_record(struct module *mod, double st_array[], U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[3] * cur_array[0];
    st_array[1] = (cur_array[2] - cur_array[1]) * cur_array[0];
    st_array[2] = cur_array[2] * cur_array[0];

    U_64 used = cur_array[2] - cur_array[1];
    U_64 nonroot_total = cur_array[2] - cur_array[1] + cur_array[3];

    if(nonroot_total != 0) {
        st_array[3]= (used * 100.0) / nonroot_total + ((used * 100) % nonroot_total != 0);
    }
    if (st_array[3] > 100) {
        st_array[3] = 100;
    }
    st_array[4] = cur_array[4];
    st_array[5] = cur_array[5];
    if (cur_array[5] >= cur_array[4] && cur_array[5] != 0) {
        st_array[6] = (cur_array[5] - cur_array[4]) * 100.0 / cur_array[5];
    }
}

static struct mod_info part_info[] = {
    {" bfree", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
    {" bused", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {" btotl", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  util", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {" ifree", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {" itotl", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
    {" iutil", DETAIL_BIT, MERGE_SUM,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--partition", partition_usage, part_info, 7, read_partition_stat, set_part_record);
}

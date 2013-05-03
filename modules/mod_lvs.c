#define   _GNU_SOURCE
#include "tsar.h"

#define LVS_STATS "/proc/net/ip_vs_stats"
#define KEEPALIVE "/var/run/keepalived.pid"
#define LVS_STORE_FMT(d)            \
    "%ld"d"%ld"d"%ld"d"%ld"d"%ld"d"%ld"
#define MAX_LINE_LEN 1024
struct stats_lvs{
    unsigned long stat;
    unsigned long conns;
    unsigned long pktin;
    unsigned long pktout;
    unsigned long bytin;
    unsigned long bytout;
};
#define STATS_LVS_SIZE       (sizeof(struct stats_lvs))

static struct mod_info info[] = {
    {"  stat", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {" conns", SUMMARY_BIT,  MERGE_NULL,  STATS_NULL},
    {" pktin", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {"pktout", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {" bytin", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {"bytout", DETAIL_BIT,  MERGE_NULL,  STATS_NULL}
};

static char *lvs_usage = "    --lvs               lvs connections and packets and bytes in/out";
struct stats_lvs st_lvs;
/*
 *******************************************************
 * Read swapping statistics from ip_vs_stat
 *******************************************************
 */
static void
read_lvs(struct module *mod)
{
    st_lvs.stat = 0;
    st_lvs.conns = 0;
    st_lvs.pktin = 0;
    st_lvs.pktout = 0;
    st_lvs.bytin = 0;
    st_lvs.bytout = 0;

    int    i=0, pos=0;
    char   buf[512];
    FILE  *fp;
    char   line[MAX_LINE_LEN];

    if (access(KEEPALIVE, 0) == 0) {
        if ((fp = fopen(LVS_STATS, "r")) != NULL) {
            while (fgets(line, MAX_LINE_LEN, fp) != NULL) {
                i++;
                if (i == 6) {
                    sscanf(line, "%lx %lx %lx %lx %lx", &st_lvs.conns, &st_lvs.pktin, &st_lvs.pktout,&st_lvs.bytin,&st_lvs.bytout);
                }
                st_lvs.stat = 1;
            }
            if (fclose(fp) < 0) {
                return;
            }
        }
    }
    if (st_lvs.stat == 1) {
        pos = sprintf(buf, LVS_STORE_FMT(DATA_SPLIT), st_lvs.stat, st_lvs.conns, st_lvs.pktin, st_lvs.pktout, st_lvs.bytin, st_lvs.bytout);
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    return;
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--lvs", lvs_usage, info, sizeof(info)/sizeof(struct mod_info), read_lvs, NULL);
}

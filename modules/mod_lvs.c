#define   _GNU_SOURCE
#include "tsar.h"

#define LVS_STATS "/proc/net/ip_vs_stats"
#define LVS_STORE_FMT(d)            \
    "%lld"d"%lld"d"%lld"d"%lld"d"%lld"d"%lld"
#define MAX_LINE_LEN 1024
struct stats_lvs{
    unsigned long long stat;
    unsigned long long conns;
    unsigned long long pktin;
    unsigned long long pktout;
    unsigned long long bytin;
    unsigned long long bytout;
};
#define STATS_LVS_SIZE       (sizeof(struct stats_lvs))

static struct mod_info info[] = {
    {"  stat", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
    {" conns", SUMMARY_BIT,  MERGE_NULL,  STATS_SUB_INTER},
    {" pktin", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
    {"pktout", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
    {" bytin", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
    {"bytout", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER}
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

    int    i = 0, pos=0;
    char   buf[512];
    char   tmp[5][16];
    FILE  *fp;
    char   line[MAX_LINE_LEN];

    if ((fp = fopen(LVS_STATS, "r")) != NULL) {
        st_lvs.stat = 1;
        while (fgets(line, MAX_LINE_LEN, fp) != NULL) {
            i++;
            if (i < 3) {
                continue;
            }
            if (!strncmp(line, "CPU", 3)) {
                /* CPU 0:       5462458943          44712664864          54084995692        8542115117674       41738811918899 */
                int    k = 0;
                k = strcspn(line, ":");
                sscanf(line + k + 1, "%s %s %s %s %s", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4]);
                st_lvs.conns += strtoll(tmp[0], NULL, 10);
                st_lvs.pktin += strtoll(tmp[1], NULL, 10);
                st_lvs.pktout += strtoll(tmp[2], NULL, 10);
                st_lvs.bytin += strtoll(tmp[3], NULL, 10);
                st_lvs.bytout += strtoll(tmp[4], NULL, 10);
            } else {
                /* 218EEA1A 1B3BA96D        0    163142140FA1F                0 */
                sscanf(line, "%s %s %s %s %s", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4]);
                st_lvs.conns += strtoll(tmp[0], NULL ,16);
                st_lvs.pktin += strtoll(tmp[1], NULL, 16);
                st_lvs.pktout += strtoll(tmp[2], NULL, 16);
                st_lvs.bytin += strtoll(tmp[3], NULL, 16);
                st_lvs.bytout += strtoll(tmp[4], NULL, 16);
                break;
            }
        }
        if (fclose(fp) < 0) {
            return;
        }
    }
    if (st_lvs.stat == 1) {
        pos = sprintf(buf, LVS_STORE_FMT(DATA_SPLIT), st_lvs.stat, st_lvs.conns, st_lvs.pktin, st_lvs.pktout, st_lvs.bytin, st_lvs.bytout);
    } else {
        return;
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    return;
}

void
set_lvs_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[0];
    int i = 1;
    for(i = 1;i<=5;i++){
        if(cur_array[i] < pre_array[i]){
            continue;
        }
        st_array[i] = (cur_array[i] -  pre_array[i]) / inter;
    }
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--lvs", lvs_usage, info, sizeof(info)/sizeof(struct mod_info), read_lvs, set_lvs_record);
}

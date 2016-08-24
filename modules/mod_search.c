#define _GNU_SOURCE


#include "tsar.h"

static char *search_usage = "    --search            KGB search statistics";
static const char * SEARCH_FILE_1 = "/tmp/_tsar_amonitor_search_1.out";
static const char * SEARCH_FILE_2 = "/tmp/_tsar_amonitor_search_2.out";

static struct mod_info search_info[] = {
    {"    rt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   qps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  fail", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" empty", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  rkrt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" rkqps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  rkto", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"rkfail", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  uprt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" upqps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
};

struct stats_search {
    double rt;
    int rt_count;
    double qps;
    int qps_count;
    double fail;
    int fail_count;
    double empty;
    int empty_count;
    double rank_rt;
    int rank_rt_count;
    double rank_qps;
    int rank_qps_count;
    double rank_to;
    int rank_to_count;
    double rank_fail;
    int rank_fail_count;
    double uprt;
    int uprt_count;
    double upqps;
    int upqps_count;
};

static struct stats_search search_stat;

static void
read_search_record(struct module *mod)
{
    int ret = 0;
    char node[LEN_1024], cmd[LEN_1024], line[LEN_1024], buf[LEN_1M];
    FILE *fp = NULL;
    char *p = NULL;
    int idx = 0;
    double f;
    ret = system("ps -ef | grep amonitor_agent | grep -v grep > /dev/null");
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;

    snprintf(cmd, LEN_1024, "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -r node | /bin/grep /master/ | /usr/bin/head -n 1 > %s", SEARCH_FILE_1);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(SEARCH_FILE_1, "r");
    if(fp == NULL)
        return;
    p = fgets(node, LEN_1024, fp);
    fclose(fp);
    fp = NULL;
    if(p == NULL)
        return;
    p = strrchr(node, '/');
    *p = 0;
    sprintf(cmd, "rm -rf %s", SEARCH_FILE_1);
    system(cmd);
    snprintf(cmd, LEN_1024, "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -n %s -m 'rt;qps;fail;empty;rank_rt;rank_qps;rank_to;rank_fail' -r metric -b -62 > %s", node, SEARCH_FILE_2);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;

    snprintf(cmd, LEN_1024, "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -n updated-adt_adgroup -m 'rt;qps' -r metric -b -62 >> %s", SEARCH_FILE_2);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(SEARCH_FILE_2, "r");
    fp = fopen(SEARCH_FILE_2, "r");
    if(fp == NULL)
        return;
    memset(&search_stat, 0, sizeof(search_stat));
    while (fgets(line, LEN_1024, fp) != NULL) {
        p = strrchr(line, '/');
        if(p != NULL) {
            if((strstr(line, "updated-adt_adgroup") != NULL)
                && (!strncmp(p+1, "rt", 2)))
                idx = 8;
            else if((strstr(line, "updated-adt_adgroup") != NULL)
                && (!strncmp(p+1, "qps", 3)))
                idx = 9;
            else if(!strncmp(p+1, "rt", 2))
                idx = 0;
            else if(!strncmp(p+1, "qps", 3))
                idx = 1;
            else if(!strncmp(p+1, "fail", 4))
                idx = 2;
            else if(!strncmp(p+1, "empty", 5))
                idx = 3;
            else if(!strncmp(p+1, "rank_rt", 7))
                idx = 4;
            else if(!strncmp(p+1, "rank_qps", 8))
                idx = 5;
            else if(!strncmp(p+1, "rank_to", 7))
                idx = 6;
            else if(!strncmp(p+1, "rank_fail", 9))
                idx = 7;
        }
        else {
            if(idx == 0) {
                sscanf(line + 24, "%lf", &f);
                search_stat.rt += f;
                search_stat.rt_count++;
            }
            else if(idx == 1) {
                sscanf(line + 24, "%lf", &f);
                search_stat.qps += f;
                search_stat.qps_count++;
            }
            else if(idx == 2) {
                sscanf(line + 24, "%lf", &f);
                search_stat.fail += f;
                search_stat.fail_count++;
            }
            else if(idx == 3) {
                sscanf(line + 24, "%lf", &f);
                search_stat.empty += f;
                search_stat.empty_count++;
            }
            else if(idx == 4) {
                sscanf(line + 24, "%lf", &f);
                search_stat.rank_rt += f;
                search_stat.rank_rt_count++;
            }
            else if(idx == 5) {
                sscanf(line + 24, "%lf", &f);
                search_stat.rank_qps += f;
                search_stat.rank_qps_count++;
            }
            else if(idx == 6) {
                sscanf(line + 24, "%lf", &f);
                search_stat.rank_to += f;
                search_stat.rank_to_count++;
            }
            else if(idx == 7) {
                sscanf(line + 24, "%lf", &f);
                search_stat.rank_fail += f;
                search_stat.rank_fail_count++;
            }
            else if(idx == 8) {
                sscanf(line + 24, "%lf", &f);
                search_stat.uprt += f;
                search_stat.uprt_count++;
            }
            else if(idx == 9) {
                sscanf(line + 24, "%lf", &f);
                search_stat.upqps += f;
                search_stat.upqps_count++;
            }
        }
    }
    fclose(fp);
    fp = NULL;
    sprintf(cmd, "rm -rf %s", SEARCH_FILE_2);
    system(cmd);

    long long rt = (search_stat.rt_count > 0) ? (long long)search_stat.rt*100/search_stat.rt_count : 0;
    long long qps = (search_stat.qps_count > 0) ? (long long)search_stat.qps*100/search_stat.qps_count : 0;
    long long fail = (search_stat.fail_count > 0) ? (long long)search_stat.fail*100/search_stat.fail_count : 0;
    long long empty = (search_stat.empty_count > 0) ? (long long)search_stat.empty*100/search_stat.empty_count : 0;
    long long rank_rt = (search_stat.rank_rt_count > 0) ? (long long)search_stat.rank_rt*100/search_stat.rank_rt_count : 0;
    long long rank_qps = (search_stat.rank_qps_count > 0) ? (long long)search_stat.rank_qps*100/search_stat.rank_qps_count : 0;
    long long rank_to = (search_stat.rank_to_count > 0) ? (long long)search_stat.rank_to*100/search_stat.rank_to_count : 0;
    long long rank_fail = (search_stat.rank_fail_count > 0) ? (long long)search_stat.rank_fail*100/search_stat.rank_fail_count : 0;
    long long uprt = (search_stat.uprt_count > 0) ? (long long)search_stat.uprt*100/search_stat.uprt_count : 0;
    long long upqps = (search_stat.upqps_count > 0) ? (long long)search_stat.upqps*100/search_stat.upqps_count : 0;
    snprintf(buf, LEN_1M, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld", rt, qps, fail, empty,rank_rt, rank_qps, rank_to, rank_fail, uprt, upqps);

    set_mod_record(mod, buf);
}

static void
set_search_record(struct module *mod, double st_array[],
        U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i = 0;
    for(; i < 8; ++i)
        st_array[i] = cur_array[i] * 1.0/100;
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--search", search_usage, search_info, 8, read_search_record, set_search_record);
}

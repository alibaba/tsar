/*
 * This module is developed and used at Alimama KGB team.
 * It collects and reports response time and throughput
 * metirics for various connected modules including merger,
 * search node, data node, user center and query rewrite.
 *
 * Author: haitao.lht@taobao.com
 */

#define _GNU_SOURCE

#include "tsar.h"

static char *merger_usage = "    --merger           KGB merger statistics";
static const char *MERGER_FILE_1 = "/tmp/_tsar_amonitor_merger_1.out";
static const char *MERGER_FILE_2 = "/tmp/_tsar_amonitor_merger_2.out";

static struct mod_info merger_info[] = {
        {"    rt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"   qps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"  fail", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" empty", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"  qrrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" qrqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"qrsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"  ucrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" ucqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"ucsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"  snrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" snqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"snsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"  dnrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" dnqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"dnsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {" tsnrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tsnqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tsnsuc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
};

struct stats_merger {
    double rt;
    int rt_count;
    double qps;
    int qps_count;
    double fail;
    int fail_count;
    double empty;
    int empty_count;
    double qr_rt;
    int qr_rt_count;
    double qr_qps;
    int qr_qps_count;
    double qr_succ;
    int qr_succ_count;
    double uc_rt;
    int uc_rt_count;
    double uc_qps;
    int uc_qps_count;
    double uc_succ;
    int uc_succ_count;
    double sn_rt;
    int sn_rt_count;
    double sn_qps;
    int sn_qps_count;
    double sn_succ;
    int sn_succ_count;
    double dn_rt;
    int dn_rt_count;
    double dn_qps;
    int dn_qps_count;
    double dn_succ;
    int dn_succ_count;
    double twins_sn_rt;
    int twins_sn_rt_count;
    double twins_sn_qps;
    int twins_sn_qps_count;
    double twins_sn_succ;
    int twins_sn_succ_count;
};

static struct stats_merger merger_stat;

static void
read_merger_record(struct module *mod) {
    int ret = 0;
    char node[LEN_1024], cmd[LEN_1024], line[LEN_1024], buf[LEN_1M];
    FILE *fp = NULL;
    char *p = NULL;
    int idx = 0;
    double f;

    ret = system("ps -ef | grep amonitor_agent | grep -v grep > /dev/null");
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    snprintf(cmd,
             LEN_1024,
             "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -r node | /bin/grep merger/ | /bin/grep -v merger/default | /usr/bin/head -n 1 > %s",
             MERGER_FILE_1);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(MERGER_FILE_1, "r");
    if (fp == NULL)
        return;
    p = fgets(node, LEN_1024, fp);
    fclose(fp);
    fp = NULL;
    if (p == NULL)
        return;
    p = strrchr(node, '/');
    *p = 0;
    sprintf(cmd,
            "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -n %s -m 'response_time;query;failure_result;empty_result;qr_succ_response_time;qr_query;qr_succ_query;uc_response_time;uc_query;uc_succ_query;kw_succ_response_time;kw_query;kw_succ_query;dn_succ_response_time;dn_query;dn_succ_query;kw_twins_succ_response_time;kw_twins_query;kw_twins_succ_query' -r metric -b -62 > %s",
            node,
            MERGER_FILE_2);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(MERGER_FILE_2, "r");
    if (fp == NULL)
        return;
    memset(&merger_stat, 0, sizeof(merger_stat));
    while (fgets(line, LEN_1024, fp) != NULL) {
        p = strrchr(line, '/');
        if (p != NULL) {
            if (!strncmp(p + 1, "response_time", 13))
                idx = 0;
            else if (!strncmp(p + 1, "query", 5))
                idx = 1;
            else if (!strncmp(p + 1, "failure_result", 14))
                idx = 2;
            else if (!strncmp(p + 1, "empty_result", 12))
                idx = 3;
            else if (!strncmp(p + 1, "qr_succ_response_time", 21))
                idx = 4;
            else if (!strncmp(p + 1, "qr_query", 8))
                idx = 5;
            else if (!strncmp(p + 1, "qr_succ_query", 13))
                idx = 6;
            else if (!strncmp(p + 1, "uc_response_time", 16))
                idx = 7;
            else if (!strncmp(p + 1, "uc_query", 8))
                idx = 8;
            else if (!strncmp(p + 1, "uc_succ_query", 13))
                idx = 9;
            else if (!strncmp(p + 1, "kw_succ_response_time", 21))
                idx = 10;
            else if (!strncmp(p + 1, "kw_query", 8))
                idx = 11;
            else if (!strncmp(p + 1, "kw_succ_query", 13))
                idx = 12;
            else if (!strncmp(p + 1, "dn_succ_response_time", 21))
                idx = 13;
            else if (!strncmp(p + 1, "dn_query", 8))
                idx = 14;
            else if (!strncmp(p + 1, "dn_succ_query", 13))
                idx = 15;
            else if (!strncmp(p + 1, "kw_twins_succ_response_time", 27))
                idx = 16;
            else if (!strncmp(p + 1, "kw_twins_query", 14))
                idx = 17;
            else if (!strncmp(p + 1, "kw_twins_succ_query", 19))
                idx = 18;
        }
        else {
            if (idx == 0) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.rt += f;
                merger_stat.rt_count++;
            }
            else if (idx == 1) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.qps += f;
                merger_stat.qps_count++;
            }
            else if (idx == 2) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.fail += f;
                merger_stat.fail_count++;
            }
            else if (idx == 3) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.empty += f;
                merger_stat.empty_count++;
            }
            else if (idx == 4) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.qr_rt += f;
                merger_stat.qr_rt_count++;
            }
            else if (idx == 5) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.qr_qps += f;
                merger_stat.qr_qps_count++;
            }
            else if (idx == 6) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.qr_succ += f;
                merger_stat.qr_succ_count++;
            }
            else if (idx == 7) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.uc_rt += f;
                merger_stat.uc_rt_count++;
            }
            else if (idx == 8) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.uc_qps += f;
                merger_stat.uc_qps_count++;
            }
            else if (idx == 9) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.uc_succ += f;
                merger_stat.uc_succ_count++;
            }
            else if (idx == 10) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.sn_rt += f;
                merger_stat.sn_rt_count++;
            }
            else if (idx == 11) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.sn_qps += f;
                merger_stat.sn_qps_count++;
            }
            else if (idx == 12) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.sn_succ += f;
                merger_stat.sn_succ_count++;
            }
            else if (idx == 13) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.dn_rt += f;
                merger_stat.dn_rt_count++;
            }
            else if (idx == 14) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.dn_qps += f;
                merger_stat.dn_qps_count++;
            }
            else if (idx == 15) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.dn_succ += f;
                merger_stat.dn_succ_count++;
            }
            else if (idx == 16) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.twins_sn_rt += f;
                merger_stat.twins_sn_rt_count++;
            }
            else if (idx == 17) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.twins_sn_qps += f;
                merger_stat.twins_sn_qps_count++;
            }
            else if (idx == 18) {
                sscanf(line + 24, "%lf", &f);
                merger_stat.twins_sn_succ += f;
                merger_stat.twins_sn_succ_count++;
            }
        }
    }
    fclose(fp);
    fp = NULL;
    sprintf(cmd, "rm -rf %s", MERGER_FILE_1);
    system(cmd);
    sprintf(cmd, "rm -rf %s", MERGER_FILE_2);
    system(cmd);
    if (merger_stat.rt_count != 0 && merger_stat.qps_count != 0 && merger_stat.fail_count != 0 &&
        merger_stat.empty_count != 0 && merger_stat.qr_rt_count != 0 && merger_stat.qr_qps_count != 0 &&
        merger_stat.qr_succ_count != 0 && merger_stat.uc_rt_count != 0 && merger_stat.uc_qps_count != 0 &&
        merger_stat.uc_succ_count != 0 && merger_stat.sn_rt_count != 0 && merger_stat.sn_qps_count != 0 &&
        merger_stat.sn_succ_count != 0 && merger_stat.dn_rt_count != 0 && merger_stat.dn_qps_count != 0 &&
        merger_stat.dn_succ_count != 0 && merger_stat.twins_sn_rt_count != 0 && merger_stat.twins_sn_qps_count != 0 &&
        merger_stat.twins_sn_succ_count != 0) {
        snprintf(buf,
                 LEN_1M,
                 "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                 (long long) merger_stat.rt * 100 / merger_stat.rt_count,
                 (long long) merger_stat.qps * 100 / merger_stat.qps_count,
                 (long long) merger_stat.fail * 100 / merger_stat.fail_count,
                 (long long) merger_stat.empty * 100 / merger_stat.empty_count,
                 (long long) merger_stat.qr_rt * 100 / merger_stat.qr_rt_count,
                 (long long) merger_stat.qr_qps * 100 / merger_stat.qr_qps_count,
                 (long long) merger_stat.qr_succ * 100 / merger_stat.qr_succ_count,
                 (long long) merger_stat.uc_rt * 100 / merger_stat.uc_rt_count,
                 (long long) merger_stat.uc_qps * 100 / merger_stat.uc_qps_count,
                 (long long) merger_stat.uc_succ * 100 / merger_stat.uc_succ_count,
                 (long long) merger_stat.sn_rt * 100 / merger_stat.sn_rt_count,
                 (long long) merger_stat.sn_qps * 100 / merger_stat.sn_qps_count,
                 (long long) merger_stat.sn_succ * 100 / merger_stat.sn_succ_count,
                 (long long) merger_stat.dn_rt * 100 / merger_stat.dn_rt_count,
                 (long long) merger_stat.dn_qps * 100 / merger_stat.dn_qps_count,
                 (long long) merger_stat.dn_succ * 100 / merger_stat.dn_succ_count,
                 (long long) merger_stat.twins_sn_rt * 100 / merger_stat.twins_sn_rt_count,
                 (long long) merger_stat.twins_sn_qps * 100 / merger_stat.twins_sn_qps_count,
                 (long long) merger_stat.twins_sn_succ * 100 / merger_stat.twins_sn_succ_count);
        set_mod_record(mod, buf);
    }
}

static void
set_merger_record(struct module *mod, double st_array[],
                  U_64 pre_array[], U_64 cur_array[], int inter) {
    int i = 0;
    for (; i < 19; ++i)
        st_array[i] = cur_array[i] * 1.0 / 100;
}

void
mod_register(struct module *mod) {
    register_mod_fields(mod,
                        "--merger",
                        merger_usage,
                        merger_info,
                        19,
                        read_merger_record,
                        set_merger_record);
}

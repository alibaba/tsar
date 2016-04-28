/*
 * This module is developed and used at Alimama KGB team.
 * It collects and reports response time and throughput
 * metirics for various connected modules including merger,
 * search node, data node, user center and query rewrite.
 *
 * Author: luoge.zg@alibaba-inc.com
 */

#define _GNU_SOURCE

#include "tsar.h"

static char *qr_usage = " --qr KGB qr statistics";
static const char *QR_FILE_1 = "/tmp/_tsar_amonitor_qr_1.out";
static const char *QR_FILE_2 = "/tmp/_tsar_amonitor_qr_2.out";

static struct mod_info qr_info[] = {
        {"qr_in", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"qr_tmot", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"qr_rt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"qr_fail", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tair_in", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tair_hit", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tair_rt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"tair_dslz", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"norm_in", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"norm_rt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"norm_fl", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
        {"rw_word", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
};

struct stats_qr {
	double qr_qps;
    int qr_qps_count;
    double qr_tmot;
    int qr_tmot_count;
    double qr_rt;
    int qr_rt_count;
    double qr_fail;
    int qr_fail_count;
	double tair_qps;
    int tair_qps_count;
    double tair_succ;
    int tair_succ_count;
	double tair_rt;
    int tair_rt_count;
    double tair_dslz;
    int tair_dslz_count;
	double norm_qps;
    int norm_qps_count;
    double norm_rt;
    int norm_rt_count;
    double norm_fail;
    int norm_fail_count;
    double rw_word;
    int rw_word_count;
};

static struct stats_qr qr_stat;

static void
read_qr_record(struct module *mod) {
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
             "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -r node | /bin/grep qr/ | /bin/grep -v qr/default | /usr/bin/head -n 1 > %s",
             QR_FILE_1);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(QR_FILE_1, "r");
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
            "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -n %s -m 'qr_in;qr_tmot;qr_rt;qr_fl;tair_in;tair_hit;tair_rt;tair_dslz;norm_in;norm_rt;norm_fl;qr_word' -r metric -b -62 > %s",
            node,
            QR_FILE_2);
    ret = system(cmd);
    if (ret == -1 || WEXITSTATUS(ret) != 0)
        return;
    fp = fopen(QR_FILE_2, "r");
    if (fp == NULL)
        return;
    memset(&qr_stat, 0, sizeof(qr_stat));
    while (fgets(line, LEN_1024, fp) != NULL) {
        p = strrchr(line, '/');
        if (p != NULL) {
            if (!strncmp(p + 1, "qr_in", 5))
                idx = 0;
            else if (!strncmp(p + 1, "q_tmot", 6))
                idx = 1;
            else if (!strncmp(p + 1, "qr_rt", 5))
                idx = 2;
            else if (!strncmp(p + 1, "qr_fl", 5))
                idx = 3;
            else if (!strncmp(p + 1, "tair_in", 7))
                idx = 4;
            else if (!strncmp(p + 1, "tair_hit", 8))
                idx = 5;
            else if (!strncmp(p + 1, "tair_rt", 7))
                idx = 6;
            else if (!strncmp(p + 1, "tair_dslz", 9))
                idx = 7;
            else if (!strncmp(p + 1, "norm_in", 7))
                idx = 8;
            else if (!strncmp(p + 1, "norm_rt", 7))
                idx = 9;
            else if (!strncmp(p + 1, "norm_fl", 7))
                idx = 10;
            else if (!strncmp(p + 1, "qr_word", 7))
                idx = 11;
        }
        else {
            if (idx == 0) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.qr_qps += f;
                qr_stat.qr_qps_count++;
            }
            else if (idx == 1) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.qr_tmot += f;
                qr_stat.qr_tmot_count++;
            }
            else if (idx == 2) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.qr_rt += f;
                qr_stat.qr_rt_count++;
            }
            else if (idx == 3) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.qr_fail += f;
                qr_stat.qr_fail_count++;
            }
            else if (idx == 4) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.tair_qps += f;
                qr_stat.tair_qps_count++;
            }
            else if (idx == 5) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.tair_succ += f;
                qr_stat.tair_succ_count++;
            }
            else if (idx == 6) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.tair_rt += f;
                qr_stat.tair_rt_count++;
            }
            else if (idx == 7) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.tair_dslz += f;
                qr_stat.tair_dslz_count++;
            }
            else if (idx == 8) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.norm_qps += f;
                qr_stat.norm_qps_count++;
            }
            else if (idx == 9) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.norm_rt += f;
                qr_stat.norm_rt_count++;
            }
            else if (idx == 10) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.norm_fail += f;
                qr_stat.norm_fail_count++;
            }
            else if (idx == 11) {
                sscanf(line + 24, "%lf", &f);
                qr_stat.rw_word += f;
                qr_stat.rw_word_count++;
            }
        }
    }
    fclose(fp);
    fp = NULL;
    sprintf(cmd, "rm -rf %s", QR_FILE_1);
    system(cmd);
    sprintf(cmd, "rm -rf %s", QR_FILE_2);
    system(cmd);
    if (qr_stat.qr_qps_count != 0 && qr_stat.qr_tmot_count != 0 && qr_stat.qr_rt_count != 0 &&
        qr_stat.qr_fail_count != 0 && qr_stat.tair_qps_count != 0 && qr_stat.tair_succ_count != 0 &&
        qr_stat.tair_rt_count != 0 && qr_stat.tair_dslz_count != 0 && qr_stat.norm_qps_count != 0 && 
        qr_stat.norm_rt_count != 0 && qr_stat.norm_fail_count != 0 && qr_stat.rw_word_count != 0) {
        snprintf(buf,
                 LEN_1M,
                 "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                 (long long) qr_stat.qr_qps* 100 / qr_stat.qr_qps_count,
                 (long long) qr_stat.qr_tmot * 100 / qr_stat.qr_tmot_count,
                 (long long) qr_stat.qr_rt * 100 / qr_stat.qr_rt_count,
                 (long long) qr_stat.qr_fail * 100 / qr_stat.qr_fail_count,
				 (long long) qr_stat.tair_qps * 100 / qr_stat.tair_qps_count,
				 (long long) qr_stat.tair_succ * 100 / qr_stat.tair_succ_count,
                 (long long) qr_stat.tair_rt * 100 / qr_stat.tair_rt_count,
                 (long long) qr_stat.tair_dslz * 100 / qr_stat.tair_dslz_count,
                 (long long) qr_stat.norm_qps * 100 / qr_stat.norm_qps_count,
                 (long long) qr_stat.norm_rt * 100 / qr_stat.norm_rt_count,
                 (long long) qr_stat.norm_fail * 100 / qr_stat.norm_fail_count,
                 (long long) qr_stat.rw_word * 100 / qr_stat.rw_word_count);
        set_mod_record(mod, buf);
    }
}

static void
set_qr_record(struct module *mod, double st_array[],
                  U_64 pre_array[], U_64 cur_array[], int inter) {
    int i = 0;
    for (; i < 16; ++i)
        st_array[i] = cur_array[i] * 1.0 / 100;
}

void
mod_register(struct module *mod) {
    register_mod_fields(mod,
                        "--qr",
                        qr_usage,
                        qr_info,
                        12,
                        read_qr_record,
                        set_qr_record);
}

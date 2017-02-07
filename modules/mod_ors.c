/*
 * This module is developed and used at Alimama KGB team.
 * It collects and reports response time and throughput
 * metirics for various connected modules including rss,
 * sn, qscore, hurricane and sib.
 *
 * Author: peijun.ypj@alibaba-inc.com
 */

#define _GNU_SOURCE

#include "tsar.h"

static char* ors_usage = "    --ors           KGB ors statistics";

static struct mod_info ors_info[] = {
    // total
    // response time, query per second, instance per second, success rate
    {"    rt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"   qps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"   ips", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"  succ", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

    // rss
    {" rssrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"rssqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"rssucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

    // sn
    {"  snrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" snqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" snips", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"snsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

    // qscore
    {"  qsrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" qsqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" qsips", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"qssucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

    // hurricane
    {"  hcrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" hcqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {" hcips", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"hcsucc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

    // sib
    {" sibrt", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"sibqps", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"sibips", SUMMARY_BIT, MERGE_SUM, STATS_NULL},
    {"sibsuc", SUMMARY_BIT, MERGE_SUM, STATS_NULL},

};

struct stats_ors {
  double rt;
  int rt_count;
  double qps;
  int qps_count;
  double ips;
  int ips_count;
  double succ;
  int succ_count;

  double rss_rt;
  int rss_rt_count;
  double rss_qps;
  int rss_qps_count;
  double rss_succ;
  int rss_succ_count;

  double sn_rt;
  int sn_rt_count;
  double sn_qps;
  int sn_qps_count;
  double sn_ips;
  int sn_ips_count;
  double sn_succ;
  int sn_succ_count;

  double qscore_rt;
  int qscore_rt_count;
  double qscore_qps;
  int qscore_qps_count;
  double qscore_ips;
  int qscore_ips_count;
  double qscore_succ;
  int qscore_succ_count;

  double hurricane_rt;
  int hurricane_rt_count;
  double hurricane_qps;
  int hurricane_qps_count;
  double hurricane_ips;
  int hurricane_ips_count;
  double hurricane_succ;
  int hurricane_succ_count;

  double sib_rt;
  int sib_rt_count;
  double sib_qps;
  int sib_qps_count;
  double sib_ips;
  int sib_ips_count;
  double sib_succ;
  int sib_succ_count;
};

static struct stats_ors ors_stat;

/***
 * main function
 *
 * @param mod
 */
static void read_ors_record(struct module* mod) {
  char node[LEN_1024], cmd[LEN_1024], line[LEN_1024], buf[LEN_1M];
  FILE* fp = NULL;
  char* p = NULL;
  int idx = 0;
  double f;

  memset(node, '\0', sizeof(node));
  memset(cmd, '\0', sizeof(cmd));
  memset(line, '\0', sizeof(line));
  memset(buf, '\0', sizeof(buf));

  snprintf(cmd,
           LEN_1024,
           "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -r node | /bin/grep ors/ | /bin/grep -v pc_ors/ | /bin/grep -v ors/default | /usr/bin/head -n 1");
  fp = popen(cmd, "r");
  if (fp == NULL) {
    return;
  }

  p = fgets(node, LEN_1024, fp);
  pclose(fp);
  fp = NULL;

  if (p == NULL) {
    return;
  }
  p = strrchr(node, '/');
  *p = 0;

  sprintf(cmd,
          "/usr/local/bin/amonitor q -a localhost:10086 -s kgb -n %s -m 'response_time;query;instance;success_query;rss_response_time;rss_query;rss_success_query;sn_response_time;sn_query;sn_instance;sn_success_query;qscore_response_time;qscore_query;qscore_instance;qscore_success_query;hurricane_response_time;hurricane_query;hurricane_instance;hurricane_success_query;sib_response_time;sib_query;sib_instance;sib_success_query' -r metric -b -62",
          node);
  fp = popen(cmd, "r");
  if (fp == NULL) {
    return;
  }

  memset(&ors_stat, 0, sizeof(ors_stat));
  while (fgets(line, LEN_1024, fp) != NULL) {
    p = strrchr(line, '/');
    if (p != NULL) {

      if (!strncmp(p + 1, "response_time", 13))
        idx = 0;
      else if (!strncmp(p + 1, "query", 5))
        idx = 1;
      else if (!strncmp(p + 1, "instance", 8))
        idx = 2;
      else if (!strncmp(p + 1, "success_query", 13))
        idx = 3;

      else if (!strncmp(p + 1, "rss_response_time", 17))
        idx = 4;
      else if (!strncmp(p + 1, "rss_query", 9))
        idx = 5;
      else if (!strncmp(p + 1, "rss_success_query", 17))
        idx = 6;

      else if (!strncmp(p + 1, "sn_response_time", 16))
        idx = 7;
      else if (!strncmp(p + 1, "sn_query", 8))
        idx = 8;
      else if (!strncmp(p + 1, "sn_instance", 11))
        idx = 9;
      else if (!strncmp(p + 1, "sn_success_query", 16))
        idx = 10;

      else if (!strncmp(p + 1, "qscore_response_time", 20))
        idx = 11;
      else if (!strncmp(p + 1, "qscore_query", 12))
        idx = 12;
      else if (!strncmp(p + 1, "qscore_instance", 15))
        idx = 13;
      else if (!strncmp(p + 1, "qscore_success_query", 20))
        idx = 14;

      else if (!strncmp(p + 1, "hurricane_response_time", 23))
        idx = 15;
      else if (!strncmp(p + 1, "hurricane_query", 15))
        idx = 16;
      else if (!strncmp(p + 1, "hurricane_instance", 18))
        idx = 17;
      else if (!strncmp(p + 1, "hurricane_success_query", 23))
        idx = 18;

      else if (!strncmp(p + 1, "sib_response_time", 17))
        idx = 19;
      else if (!strncmp(p + 1, "sib_query", 9))
        idx = 20;
      else if (!strncmp(p + 1, "sib_instance", 12))
        idx = 21;
      else if (!strncmp(p + 1, "sib_success_query", 17))
        idx = 22;

    } else {

      if (idx == 0) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.rt += f;
        ors_stat.rt_count++;
      } else if (idx == 1) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.qps += f;
        ors_stat.qps_count++;
      } else if (idx == 2) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.ips += f;
        ors_stat.ips_count++;
      } else if (idx == 3) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.succ += f;
        ors_stat.succ_count++;

      } else if (idx == 4) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.rss_rt += f;
        ors_stat.rss_rt_count++;
      } else if (idx == 5) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.rss_qps += f;
        ors_stat.rss_qps_count++;
      } else if (idx == 6) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.rss_succ += f;
        ors_stat.rss_succ_count++;

      } else if (idx == 7) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sn_rt += f;
        ors_stat.sn_rt_count++;
      } else if (idx == 8) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sn_qps += f;
        ors_stat.sn_qps_count++;
      } else if (idx == 9) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sn_ips += f;
        ors_stat.sn_ips_count++;
      } else if (idx == 10) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sn_succ += f;
        ors_stat.sn_succ_count++;

      } else if (idx == 11) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.qscore_rt += f;
        ors_stat.qscore_rt_count++;
      } else if (idx == 12) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.qscore_qps += f;
        ors_stat.qscore_qps_count++;
      } else if (idx == 13) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.qscore_ips += f;
        ors_stat.qscore_ips_count++;
      } else if (idx == 14) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.qscore_succ += f;
        ors_stat.qscore_succ_count++;

      } else if (idx == 15) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.hurricane_rt += f;
        ors_stat.hurricane_rt_count++;
      } else if (idx == 16) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.hurricane_qps += f;
        ors_stat.hurricane_qps_count++;
      } else if (idx == 17) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.hurricane_ips += f;
        ors_stat.hurricane_ips_count++;
      } else if (idx == 18) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.hurricane_succ += f;
        ors_stat.hurricane_succ_count++;

      } else if (idx == 19) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sib_rt += f;
        ors_stat.sib_rt_count++;
      } else if (idx == 20) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sib_qps += f;
        ors_stat.sib_qps_count++;
      } else if (idx == 21) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sib_ips += f;
        ors_stat.sib_ips_count++;
      } else if (idx == 22) {
        sscanf(line + 24, "%lf", &f);
        ors_stat.sib_succ += f;
        ors_stat.sib_succ_count++;
      }

    }
  }

  pclose(fp);
  fp = NULL;

  if (ors_stat.rt_count != 0
      && ors_stat.qps_count != 0
      && ors_stat.ips_count != 0
      && ors_stat.succ_count != 0

      && ors_stat.rss_rt_count != 0
      && ors_stat.rss_qps_count != 0
      && ors_stat.rss_succ_count != 0

      && ors_stat.sn_rt_count != 0
      && ors_stat.sn_qps_count != 0
      && ors_stat.sn_ips_count != 0
      && ors_stat.sn_succ_count != 0

      && ors_stat.qscore_rt_count != 0
      && ors_stat.qscore_qps_count != 0
      && ors_stat.qscore_ips_count != 0
      && ors_stat.qscore_succ_count != 0

      && ors_stat.hurricane_rt_count != 0
      && ors_stat.hurricane_qps_count != 0
      && ors_stat.hurricane_ips_count != 0
      && ors_stat.hurricane_succ_count != 0

      && ors_stat.sib_rt_count != 0
      && ors_stat.sib_qps_count != 0
      && ors_stat.sib_ips_count != 0
      && ors_stat.sib_succ_count != 0) {

    snprintf(buf,
             LEN_1M,
             "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
             (long long) ors_stat.rt * 100 / ors_stat.rt_count,
             (long long) ors_stat.qps * 100 / ors_stat.qps_count,
             (long long) ors_stat.ips * 100 / ors_stat.ips_count,
             (long long) ors_stat.succ * 100 / ors_stat.succ_count,

             (long long) ors_stat.rss_rt * 100 / ors_stat.rss_rt_count,
             (long long) ors_stat.rss_qps * 100 / ors_stat.rss_qps_count,
             (long long) ors_stat.rss_succ * 100 / ors_stat.rss_succ_count,

             (long long) ors_stat.sn_rt * 100 / ors_stat.sn_rt_count,
             (long long) ors_stat.sn_qps * 100 / ors_stat.sn_qps_count,
             (long long) ors_stat.sn_ips * 100 / ors_stat.sn_ips_count,
             (long long) ors_stat.sn_succ * 100 / ors_stat.sn_succ_count,

             (long long) ors_stat.qscore_rt * 100 / ors_stat.qscore_rt_count,
             (long long) ors_stat.qscore_qps * 100 / ors_stat.qscore_qps_count,
             (long long) ors_stat.qscore_ips * 100 / ors_stat.qscore_ips_count,
             (long long) ors_stat.qscore_succ * 100
                 / ors_stat.qscore_succ_count,

             (long long) ors_stat.hurricane_rt * 100
                 / ors_stat.hurricane_rt_count,
             (long long) ors_stat.hurricane_qps * 100
                 / ors_stat.hurricane_qps_count,
             (long long) ors_stat.hurricane_ips * 100
                 / ors_stat.hurricane_ips_count,
             (long long) ors_stat.hurricane_succ * 100
                 / ors_stat.hurricane_succ_count,

             (long long) ors_stat.sib_rt * 100 / ors_stat.sib_rt_count,
             (long long) ors_stat.sib_qps * 100 / ors_stat.sib_qps_count,
             (long long) ors_stat.sib_ips * 100 / ors_stat.sib_ips_count,
             (long long) ors_stat.sib_succ * 100 / ors_stat.sib_succ_count);
    set_mod_record(mod, buf);
  }
}

static void set_ors_record(struct module* mod,
                           double st_array[],
                           U_64 pre_array[],
                           U_64 cur_array[],
                           int inter) {
  int i;
  for (i = 0; i < 23; ++i) {
    st_array[i] = cur_array[i] * 1.0 / 100;
  }

}

void mod_register(struct module* mod) {
  register_mod_fields(mod,
                      "--ors",
                      ors_usage,
                      ors_info,
                      23,
                      read_ors_record,
                      set_ors_record);
}


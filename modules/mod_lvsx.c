/* Author: dancer.liy
 * Date: 2015-04-28
 * Version: 1.0
 * Changelog:
 *
 * 2015-04-28: dancer.liy
 *     1. init verion
 *
 * synproxy flags:
 *      0: disabled(enforced or auto);
 *      1: enabled, but current is off by auto algorithm
 *      2: enabled, by enforced or by auto algorithm
 */

#include "tsar.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define LVSX_STATS "/tmp/lvsx_stats"

char *lvsx_stats_usage =
"    --lvsx           lvs extended network traffic statistics";

#define LEN_64 64
#define LEN_2048 2048
#define LEN_20480 20480

struct lvsx_stats {
        char vip[LEN_64];
        char vport[LEN_64];
        unsigned long long vs_conns;
        unsigned long long vs_pktin;
        unsigned long long vs_pktout;
        unsigned long long vs_bytin;
        unsigned long long vs_bytout;
        unsigned long long vs_synproxy;
        unsigned long long vs_syncount;
        unsigned long long vs_ackcount;
        unsigned long long rson_count;
        unsigned long long rsoff_count;
};

static int prepare_data(void)
{
        FILE   *stream;
        char    buf[LEN_2048];
        int     b_data_open = FALSE;
        int     fd = 0;

        stream = fopen("/tmp/lvsx_tmp5", "r");
        if (!stream)
                return -1;
        while(fgets(buf, LEN_2048, stream)){
                if(!b_data_open){
                        remove(LVSX_STATS);
                        fd = open(LVSX_STATS, O_WRONLY|O_CREAT, 0666);
                        if(fd <= 0){
                                printf("open file %s failed. maybe access authority, try to delete it and exec again\n", LVSX_STATS);
                                return -1;
                        }
                        b_data_open = TRUE;
                }
                write(fd,buf,strlen(buf));
        }
        fclose(stream);
        if (b_data_open && fd > 0) {
                close(fd);
        }
        return 0;
}

/* Todo: change all shell code into c */
static void
read_lvsx_record(struct module *mod)
{
        FILE             *fp;
        char              line[LEN_1024];
        char              buf[LEN_20480];
        struct lvsx_stats stat, all_stat;
        int               pos = 0;
        int               is_firstline = 1;
        int               ret = 0;

        memset(buf, 0, LEN_20480);
        memset(line, 0, LEN_1024);
        memset(&stat, 0, sizeof(struct lvsx_stats));
        memset(&all_stat, 0, sizeof(struct lvsx_stats));

        ret = system("ipvsadm -ln --exact --stats >& /tmp/lvsx_tmp1");
        if (ret == -1 || WEXITSTATUS(ret) != 0)
                return;
        ret = system("ipvsadm -ln >& /tmp/lvsx_tmp2");
        if (ret == -1 || WEXITSTATUS(ret) != 0)
                return;
        ret = system("awk 'NR == FNR && /TCP/{curvs=$2;sched[$2]=$3;synproxy[$2]=$5;next;}"
               "NR == FNR && /-> [^Remote]/{idx=(curvs\" \"$2);rs[idx]=$3\" \"$4\" \"$5\" \"$6;next;}"
               "/TCP/{curvs=$2;print $0,sched[$2],synproxy[$2]}"
               "/-> [^Remote]/{idx=curvs\" \"$2;print $0,rs[idx]}'"
               " /tmp/lvsx_tmp2"
               " /tmp/lvsx_tmp1 >& /tmp/lvsx_tmp3");
        if (ret == -1 || WEXITSTATUS(ret) != 0)
                return;
        system("cat /proc/net/ip_vs >& /tmp/lvsx_tmp4");
        if (ret == -1 || WEXITSTATUS(ret) != 0)
                return;
        ret = system("awk '"
                     "function str2vs(vs, ip_port, ip) {"
                        "split(vs, ip_port, \":\");"
                        "for(i=1; i <= 6; i+=2) {"
                                "ip=ip strtonum(\"0x\" substr(ip_port[1], i, 2)) \".\";"
                        "}"
                        "ip=ip strtonum(\"0x\"substr(ip_port[1], i, 2));"
                        "port=strtonum(\"0x\"ip_port[2]);"
                        "return ip\":\"port;"
                     "}"
                     "BEGIN {"
                        "\"getconf _NPROCESSORS_ONLN\" | getline cpus;"
                        "\"sysctl -n net.ipv4.vs.synproxy_auto_switch\" | getline synproxy_auto_flags;"
                     "}"
                     "NR==FNR && /^TCP/ {"
                        "vs=str2vs($2);"
                        "next;"
                     "}"
                     "NR==FNR && /->SYNPROXY-ON/ {"
                        "split($0, f, \":\");"
                        "split(f[2], flags, \"|\");"
                        "synproxy_vs[vs]=0;"
                        "for(i=1; i<=cpus;i++) {"
                                "if (flags[i] != 0) {"
                                        "synproxy_vs[vs]=1;"
                                        "break;"
                                "}"
                        "}"
                        "next;"
                     "}"
                     "NR==FNR && /->SYN-CNT/ {"
                        "split($0, c, \":\");"
                        "split(c[2], count, \"|\");"
                        "for(i=1;i<=cpus;i++) {"
                                "syncount[vs]+=count[i];"
                        "}"
                        "next;"
                     "}"
                     "NR==FNR && /->ACK-CNT/ {"
                        "split($0, c, \":\");"
                        "split(c[2], count, \"|\");"
                        "for(i=1;i<=cpus;i++) {"
                                "ackcount[vs]+=count[i];"
                        "}"
                        "next;"
                     "}"
                     "/TCP / {"
                        "if ($9 == \"synproxy\") {"
                                "if (synproxy_auto_flags > 0) {"
                                        "if (synproxy_vs[$2] > 0) {"
                                                "synproxy=2;"
                                        "} else {"
                                                "synproxy=1;"
                                        "};"
                                "} else {"
                                        "synproxy=2;"
                                "};"
                        "} else {"
                                "synproxy=0;"
                        "};"
                        "$9=synproxy;"
                        "print $0,syncount[$2],ackcount[$2];"
                     "}"
                     "/-> [^Remote]/{"
                        "print $0;"
                     "}' /tmp/lvsx_tmp4 /tmp/lvsx_tmp3 >& /tmp/lvsx_tmp5");
        if (ret == -1 || WEXITSTATUS(ret) != 0)
                return;

        ret = prepare_data();
        if (ret < 0)
                return;

        if ((fp = fopen(LVSX_STATS, "r")) == NULL)
                return;
        while (fgets(line, LEN_1024, fp) != NULL) {
                if (!strncmp(line, "TCP", 3)) {
                        int    k = 0;

                        k = strcspn(line, " ");
                        if (is_firstline) {
                                is_firstline = 0;
                        } else {
                                pos += sprintf(buf+pos, "%s_%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu"ITEM_SPLIT,
                                               stat.vip,
                                               stat.vport,
                                               stat.vs_conns,
                                               stat.vs_pktin,
                                               stat.vs_pktout,
                                               stat.vs_bytin,
                                               stat.vs_bytout,
                                               stat.vs_synproxy,
                                               stat.vs_syncount,
                                               stat.vs_ackcount,
                                               stat.rsoff_count,
                                               stat.rson_count);
                                memset(&stat, 0, sizeof(struct lvsx_stats));
                        }
                        ret = sscanf(line + k + 1, "%[^:]:%s %llu %llu %llu %llu %llu %*s %llu %llu %llu",
                                     stat.vip,
                                     stat.vport,
                                     &stat.vs_conns,
                                     &stat.vs_pktin,
                                     &stat.vs_pktout,
                                     &stat.vs_bytin,
                                     &stat.vs_bytout,
                                     &stat.vs_synproxy,
                                     &stat.vs_syncount,
                                     &stat.vs_ackcount);
                        if (ret != 10) {
                                perror("parsing tcp line error");
                        }
                        all_stat.vs_conns += stat.vs_conns;
                        all_stat.vs_pktin += stat.vs_pktin;
                        all_stat.vs_pktout += stat.vs_pktout;
                        all_stat.vs_bytin += stat.vs_bytin;
                        all_stat.vs_bytout += stat.vs_bytout;
                        all_stat.vs_synproxy = 1;
                        all_stat.vs_syncount += stat.vs_syncount;
                        all_stat.vs_ackcount += stat.vs_ackcount;
                }else if (!strncmp(line+2, "->", 2)){
                        int weight = 0;
                        int k  = 0;

                        k = strcspn(line, " ");
                        ret = sscanf(line + 5, "%*s %*u %*u %*u %*u %*u %*s %d %*d %*d", &weight);
                        weight > 0 ? stat.rson_count++ : stat.rsoff_count++;
                        weight > 0 ? all_stat.rson_count++ : all_stat.rsoff_count++;
                }
        }
        pos += sprintf(buf+pos, "%s_%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu"ITEM_SPLIT,
                       stat.vip,
                       stat.vport,
                       stat.vs_conns,
                       stat.vs_pktin,
                       stat.vs_pktout,
                       stat.vs_bytin,
                       stat.vs_bytout,
                       stat.vs_synproxy,
                       stat.vs_syncount,
                       stat.vs_ackcount,
                       stat.rsoff_count,
                       stat.rson_count);
        pos += sprintf(buf+pos, "%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu"ITEM_SPLIT,
                       "all",
                       all_stat.vs_conns,
                       all_stat.vs_pktin,
                       all_stat.vs_pktout,
                       all_stat.vs_bytin,
                       all_stat.vs_bytout,
                       all_stat.vs_synproxy,
                       all_stat.vs_syncount,
                       all_stat.vs_ackcount,
                       all_stat.rsoff_count,
                       all_stat.rson_count);

        if (pos <= 0) {
                printf("no record\n");
                fclose(fp);
                return;
        }

        buf[pos] = '\0';
        set_mod_record(mod, buf);
        fclose(fp);
}

static void
set_lvsx_record(struct module *mod, double st_array[],
                U_64 pre_array[], U_64 cur_array[], int inter)
{
        int i;
        for (i = 0; i < 8; i++) {
                if (i == 5) {
                        /* synproxy flag */
                        st_array[i] = cur_array[i];
                        continue;
                }
                if (cur_array[i] >= pre_array[i]) {
                        st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
                } else
                        st_array[i] = 0;
        }

        for (i = 8; i < 10; i++) {
                st_array[i] = cur_array[i];
        }
}

static struct mod_info lvsx_stats_info[] = {
        {" conns", DETAIL_BIT, 0, STATS_NULL},
        {" pktin", DETAIL_BIT, 0, STATS_NULL},
        {"pktout", DETAIL_BIT, 0, STATS_NULL},
        {" bytin", DETAIL_BIT, 0, STATS_NULL},
        {"bytout", DETAIL_BIT, 0, STATS_NULL},
        {"sproxy", DETAIL_BIT, 0, STATS_NULL},
        {"syncnt", DETAIL_BIT, 0, STATS_NULL},
        {"ackcnt", DETAIL_BIT, 0, STATS_NULL},
        {" rsoff", DETAIL_BIT, 0, STATS_NULL},
        {"  rson", DETAIL_BIT, 0, STATS_NULL},
};

void
mod_register(struct module *mod)
{
        register_mod_fields(mod, "--lvsx", lvsx_stats_usage, lvsx_stats_info, 10, read_lvsx_record, set_lvsx_record);
}


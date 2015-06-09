#include "tsar.h"
#define NETWORK_STAT "/proc/net/network"

char *network_stats_usage =
"    --network           network traffic statistics";

#define NETWORK_STATS_SIZE (sizeof(struct network_stats))
#define LEN_10 10
#define NUM_FIELDS 16
#define LEN_2048 2048

struct network_stats {
        char port[LEN_10];
        unsigned long long all_in_total_pkts;
        unsigned long long all_in_total_len;
        unsigned long long all_in_tcp_pkts;
        unsigned long long all_in_syn_pkts;
        unsigned long long all_in_ack_pkts;
        unsigned long long all_in_synack_pkts;
        unsigned long long all_in_fin_pkts;
        unsigned long long all_in_rst_pkts;
        unsigned long long all_out_total_pkts;
        unsigned long long all_out_total_len;
        unsigned long long all_out_tcp_pkts;
        unsigned long long all_out_syn_pkts;
        unsigned long long all_out_ack_pkts;
        unsigned long long all_out_synack_pkts;
        unsigned long long all_out_fin_pkts;
        unsigned long long all_out_rst_pkts;
};

void
read_network_stats_record(struct module *mod)
{
        FILE             *fp;
        char              line[LEN_1024];
        char              buf[LEN_1024];
        struct network_stats stats_n;
        int              pos = 0;

        memset(buf, 0, LEN_1024);
        memset(line, 0, LEN_1024);
        memset(&stats_n, 0, sizeof(struct network_stats));
        if ((fp = fopen(NETWORK_STAT, "r")) == NULL) {
                printf("tsar : mod_network file %s cannot open!\n", NETWORK_STAT);
                return;
        }

        while (fgets(line, LEN_1024, fp) != NULL) {
                if (sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           stats_n.port,
                           &stats_n.all_in_total_pkts,
                           &stats_n.all_in_total_len,
                           &stats_n.all_in_tcp_pkts,
                           &stats_n.all_in_syn_pkts,
                           &stats_n.all_in_ack_pkts,
                           &stats_n.all_in_synack_pkts,
                           &stats_n.all_in_fin_pkts,
                           &stats_n.all_in_rst_pkts,
                           &stats_n.all_out_total_pkts,
                           &stats_n.all_out_total_len,
                           &stats_n.all_out_tcp_pkts,
                           &stats_n.all_out_syn_pkts,
                           &stats_n.all_out_ack_pkts,
                           &stats_n.all_out_synack_pkts,
                           &stats_n.all_out_fin_pkts,
                           &stats_n.all_out_rst_pkts) == 17 ) {

                        pos += sprintf(buf+pos, "port%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu"ITEM_SPLIT,
                                       stats_n.port,
                                       stats_n.all_in_total_pkts,
                                       stats_n.all_in_total_len,
                                       stats_n.all_in_tcp_pkts,
                                       stats_n.all_in_syn_pkts,
                                       stats_n.all_in_ack_pkts,
                                       stats_n.all_in_synack_pkts,
                                       stats_n.all_in_fin_pkts,
                                       stats_n.all_in_rst_pkts,
                                       stats_n.all_out_total_pkts,
                                       stats_n.all_out_total_len,
                                       stats_n.all_out_tcp_pkts,
                                       stats_n.all_out_syn_pkts,
                                       stats_n.all_out_ack_pkts,
                                       stats_n.all_out_synack_pkts,
                                       stats_n.all_out_fin_pkts,
                                       stats_n.all_out_rst_pkts);
                }
        }

        if (pos <= 0){
            fclose(fp);
            return ;
        }

        buf[pos] = '\0';
        set_mod_record(mod, buf);
        fclose(fp);
}

static void
set_network_stats_record(struct module *mod, double st_array[],
                         U_64 pre_array[], U_64 cur_array[], int inter)
{
        int i;
        for (i = 0; i < NUM_FIELDS; i++) {
                if (cur_array[i] >= pre_array[i]) {
                        st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
                } else
                        st_array[i] = 0;
        }
}

static struct mod_info network_stats_info[] = {
        {" pktin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" bytin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" tcpin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" synin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" ackin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"syakin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" finin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {" rstin" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,

        {"pktout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"bytout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"tcpout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"synout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"ackout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"syakou" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"finout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
        {"rstout" , SUMMARY_BIT , 0 , STATS_SUB_INTER} ,
};

void
mod_register(struct module *mod)
{
        register_mod_fields(mod, "--network", network_stats_usage, network_stats_info, NUM_FIELDS, read_network_stats_record, set_network_stats_record);
}

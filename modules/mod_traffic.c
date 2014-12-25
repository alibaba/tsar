#include "tsar.h"

char *traffic_usage = "    --traffic           Net traffic statistics";

/*
 * Structure for traffic infomation.
 */
struct stats_traffic {
    unsigned long long bytein;
    unsigned long long byteout;
    unsigned long long pktin;
    unsigned long long pktout;
} ;

#define STATS_TRAFFIC_SIZE (sizeof(struct stats_traffic))


/*
 * collect traffic infomation
 */
static void
read_traffic_stats(struct module *mod)
{
    int                   len = 0, num = 0;
    FILE                 *fp;
    char                 *p = NULL;
    char                  line[LEN_4096] = {0};
    char                  buf[LEN_4096] = {0};
    struct stats_traffic  total_st, cur_st;

    memset(buf, 0, LEN_4096);
    memset(&total_st, 0, sizeof(struct stats_traffic));
    memset(&cur_st, 0, sizeof(struct stats_traffic));

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    memset(&total_st, 0, sizeof(cur_st));

    while (fgets(line, LEN_4096, fp) != NULL) {
        if (strstr(line, "eth") || strstr(line, "em") || strstr(line, "venet")) {
            memset(&cur_st, 0, sizeof(cur_st));
            p = strchr(line, ':');
            sscanf(p + 1, "%llu %llu %*u %*u %*u %*u %*u %*u "
                    "%llu %llu %*u %*u %*u %*u %*u %*u",
                    &cur_st.bytein,
                    &cur_st.pktin,
                    &cur_st.byteout,
                    &cur_st.pktout);

            num++;
            total_st.bytein  += cur_st.bytein;
            total_st.byteout += cur_st.byteout;
            total_st.pktin   += cur_st.pktin;
            total_st.pktout  += cur_st.pktout;
        }
    }

    len = sprintf(buf, "%lld,%lld,%lld,%lld",
            total_st.bytein,
            total_st.byteout,
            total_st.pktin,
            total_st.pktout);
    buf[len] = '\0';
    if(num > 0) {
        set_mod_record(mod, buf);
    }
    fclose(fp);
}

static struct mod_info traffic_info[] ={
    {" bytin", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {"bytout", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {" pktin", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pktout", DETAIL_BIT,  0,  STATS_SUB_INTER}
};

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--traffic", traffic_usage, traffic_info, 4, read_traffic_stats, NULL);
}

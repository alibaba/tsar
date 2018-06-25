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
    unsigned long long pkterrin;
    unsigned long long pktdrpin;
    unsigned long long pkterrout;
    unsigned long long pktdrpout;
} ;

#define STATS_TRAFFIC_SIZE (sizeof(struct stats_traffic))


/*
 * collect traffic infomation
 */
static void
read_traffic_stats(struct module *mod, char *parameter)
{
    int                   len = 0, num = 0, n_prefix = 3, i;
    FILE                 *fp;
    char                 *p = NULL;
    char                  line[LEN_4096] = {0};
    char                  buf[LEN_4096] = {0};
    char                  mod_parameter[LEN_256], *token;
    char                 *prefixes[10] = {"eth", "em", "en"};
    struct stats_traffic  total_st, cur_st;

    memset(buf, 0, LEN_4096);
    memset(&total_st, 0, sizeof(struct stats_traffic));
    memset(&cur_st, 0, sizeof(struct stats_traffic));

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    memset(&total_st, 0, sizeof(cur_st));

    if (parameter != NULL) {
        strncpy(mod_parameter, parameter, LEN_256);
        token = strtok(mod_parameter, W_SPACE);
        while (token != NULL && n_prefix < 10) {
            prefixes[n_prefix++] = token;
            token = strtok(NULL, W_SPACE);
        }
    }

    while (fgets(line, LEN_4096, fp) != NULL) {
        for (i = 0; i < n_prefix; i++) {
            if (strstr(line, prefixes[i])) {
                memset(&cur_st, 0, sizeof(cur_st));
                p = strchr(line, ':');
                sscanf(p + 1, "%llu %llu %llu %llu %*u %*u %*u %*u "
                        "%llu %llu %llu %llu %*u %*u %*u %*u",
                        &cur_st.bytein,
                        &cur_st.pktin,
                        &cur_st.pkterrin,
                        &cur_st.pktdrpin,
                        &cur_st.byteout,
                        &cur_st.pktout,
                        &cur_st.pkterrout,
                        &cur_st.pktdrpout);

                num++;
                total_st.bytein    += cur_st.bytein;
                total_st.byteout   += cur_st.byteout;
                total_st.pktin     += cur_st.pktin;
                total_st.pktout    += cur_st.pktout;
                total_st.pkterrin  += cur_st.pkterrin;
                total_st.pktdrpin  += cur_st.pktdrpin;
                total_st.pkterrout += cur_st.pkterrout;
                total_st.pktdrpout += cur_st.pktdrpout;

                break;
            }
        }
    }

    len = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld",
            total_st.bytein,
            total_st.byteout,
            total_st.pktin,
            total_st.pktout,
            total_st.pkterrin + total_st.pkterrout,
            total_st.pktdrpin + total_st.pktdrpout);
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
    {"pktout", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pkterr", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pktdrp", DETAIL_BIT,  0,  STATS_SUB_INTER}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--traffic", traffic_usage, traffic_info, 6, read_traffic_stats, NULL);
}

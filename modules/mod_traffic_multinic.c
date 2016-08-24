#include "tsar.h"

/* 
 * usage in tsar.conf: 
 * traffic_multinic on T1 T2 eth0 eth1 ...  ----will print T1, T2, eth0, eth1 ... network adapters stats
 * or  traffic_multinic on  ----will print total network adapters stats
 */
char *multinic_usage = "    --traffic_multinic  Net traffic statistics for different network interface";

/*
 * Max number of network interface for traffic_multinic param
 */
#define MAX_NICS 12

/*
 * Structure for traffic infomation.
 */
struct stats_pernic {
    unsigned long long bytein;
    unsigned long long byteout;
    unsigned long long pktin;
    unsigned long long pktout;
    unsigned long long pkterrin;
    unsigned long long pktdrpin;
    unsigned long long pkterrout;
    unsigned long long pktdrpout;
    char name[16];
} ;

#define STATS_TRAFFIC_SIZE (sizeof(struct stats_pernic))

static int 
split(char **arr, char *str, const char *del) 
{
    int n = 0;
    char *s = strtok(str, del);
    while(s != NULL && n < MAX_NICS) {
        *arr++ = s;
        s = strtok(NULL, del);
        n++;
    }

    return n;
}


/*
 * collect traffic infomation
 */
static void
read_traffic_stats(struct module *mod, char *parameter)
{
    int pos = 0, nics = 0, i;
    FILE *fp;
    char line[LEN_1M] = {0};
    char buf[LEN_1M] = {0};
    struct stats_pernic  st_pernic, total_nics;
    char mod_parameter[LEN_256];
    char *arr[MAX_NICS] = {NULL};

    if (parameter) {
        strcpy(mod_parameter, parameter);
        nics = split(arr, mod_parameter, W_SPACE);
    }
    
    memset(buf, 0, LEN_1M);
    memset(&st_pernic, 0, sizeof(struct stats_pernic));
    memset(&total_nics, 0, sizeof(struct stats_pernic));

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_1M, fp) != NULL) {
        memset(&st_pernic, 0, sizeof(st_pernic));
        if (!strstr(line, ":")) {
            continue;
        }
        sscanf(line,  "%*[^a-zA-Z]%[^:]:%llu %llu %llu %llu %*u %*u %*u %*u "
                "%llu %llu %llu %llu %*u %*u %*u %*u",
                st_pernic.name,
                &st_pernic.bytein,
                &st_pernic.pktin,
                &st_pernic.pkterrin,
                &st_pernic.pktdrpin,
                &st_pernic.byteout,
                &st_pernic.pktout,
                &st_pernic.pkterrout,
                &st_pernic.pktdrpout);

        /* if nic not used, skip it */
        if (st_pernic.bytein == 0) {
            continue;
        }
        /* if no param used, calculate total_nics */
        if (nics == 0) {
            if (strstr(line, "eth") || strstr(line, "em") || strstr(line, "venet")) {
                total_nics.bytein    += st_pernic.bytein;
                total_nics.byteout   += st_pernic.byteout;
                total_nics.pktin     += st_pernic.pktin;
                total_nics.pktout    += st_pernic.pktout;
                total_nics.pkterrin  += st_pernic.pkterrin;
                total_nics.pktdrpin  += st_pernic.pktdrpin;
                total_nics.pkterrout += st_pernic.pkterrout;
                total_nics.pktdrpout += st_pernic.pktdrpout;
                continue;
            }
        }

        for (i = 0; i < nics; i++) {
            if (strstr(st_pernic.name, arr[i])) {
                pos += snprintf(buf + pos, LEN_1M - pos, "%s=%lld,%lld,%lld,%lld,%lld,%lld" ITEM_SPLIT,
                st_pernic.name,
                st_pernic.bytein,
                st_pernic.byteout,
                st_pernic.pktin,
                st_pernic.pktout,
                st_pernic.pkterrin + st_pernic.pkterrout,
                st_pernic.pktdrpin + st_pernic.pktdrpout);
            }
        }

        if (strlen(buf) == LEN_1M - 1) {
            fclose(fp);
            return;
        }
    }
    
    /* if no param used, print total_nics */
    if (nics == 0) {
        pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld",
        total_nics.bytein,
        total_nics.byteout,
        total_nics.pktin,
        total_nics.pktout,
        total_nics.pkterrin + total_nics.pkterrout,
        total_nics.pktdrpin + total_nics.pktdrpout);
        buf[pos] = '\0';
    }
    
    set_mod_record(mod, buf);
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
    register_mod_fields(mod, "--traffic_multinic", multinic_usage, traffic_info, 6, read_traffic_stats, NULL);
}

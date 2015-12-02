#include "tsar.h"
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define  MAX_DEV_NUM      1024

char *mtraffic_usage = "    --mtraffic          Multi interface net traffic statistics";

/*
 * Structure for traffic infomation.
 */
struct stats_traffic {
    char               dev_name[LEN_64];
    unsigned long long bytein;
    unsigned long long byteout;
    unsigned long long pktin;
    unsigned long long pktout;
    unsigned long long pkterrin;
    unsigned long long pktdrpin;
    unsigned long long pkterrout;
    unsigned long long pktdrpout;
} ;

/* Structure for tsar  */
static struct mod_info  ntraffic_info[MAX_COL_NUM];

static void
read_mtraffic_stats(struct module *mod, const char *parameter) {
    int                   devs_num = 0, len = 0;
    FILE                 *fp;
    char                 *p = NULL;
    char                  line[LEN_4096] = {0};
    char                  buf[LEN_4096] = {0};
    struct stats_traffic  devs_cur_st[MAX_DEV_NUM];

    memset(buf, 0, LEN_4096);

    devs_num = 0;

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_4096, fp) != NULL) {
        if (strstr(line, "Receive") && strstr(line, "Transmit")) continue;
        if (strstr(line, "bytes") && strstr(line, "packets")) continue;
        p = strtok(line, ":");
        int n = 0;
        for(n = 0 ; n < strlen(p); n++) {
            if (p[n] != ' ') break;
        }

        if ( strcmp(p+n, "lo") == 0 ) continue;
        sprintf(devs_cur_st[devs_num].dev_name, "%s", p+n);

        p = strtok(NULL, ":");
        sscanf(p, "%llu %llu %llu %llu %*u %*u %*u %*u "
                "%llu %llu %llu %llu %*u %*u %*u %*u",
                &devs_cur_st[devs_num].bytein,
                &devs_cur_st[devs_num].pktin,
        &devs_cur_st[devs_num].pkterrin,
        &devs_cur_st[devs_num].pktdrpin,
                &devs_cur_st[devs_num].byteout,
        &devs_cur_st[devs_num].pktout,
        &devs_cur_st[devs_num].pkterrout,
                &devs_cur_st[devs_num].pktdrpout);
        devs_num++;
    }

    fclose(fp);

    int n = 0;
    len = 0;
    for(; n < devs_num; n++) {
        int this_len = sprintf(buf + len, "%s=%lld,%lld,%lld,%lld,%lld,%lld;",
                devs_cur_st[n].dev_name,
                devs_cur_st[n].bytein,
                devs_cur_st[n].byteout,
                devs_cur_st[n].pktin,
                devs_cur_st[n].pktout,
                devs_cur_st[n].pkterrin + devs_cur_st[n].pkterrout,
                devs_cur_st[n].pktdrpin + devs_cur_st[n].pktdrpout);
        len += this_len;
    }
    buf[len] = '\0';

    if(devs_num > 0) {
        set_mod_record(mod, buf);
    }
}


static struct mod_info mtraffic_info[] ={
    {" bytin", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {"bytout", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {" pktin", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pktout", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pkterr", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"pktdrp", DETAIL_BIT,  0,  STATS_SUB_INTER}
};

static void
set_mtraffic_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter) {
    unsigned long long bytein = cur_array[0] - pre_array[0];
    unsigned long long byteout = cur_array[1] - pre_array[1];
    unsigned long long pktin = cur_array[2] - pre_array[2];
    unsigned long long pktout = cur_array[3] - pre_array[3];;
    unsigned long long pkterrin = cur_array[4] - pre_array[4];
    unsigned long long pktdrpin = cur_array[5] - pre_array[5];
    st_array[0] = bytein / (inter * 1.0);
    st_array[1] = byteout / (inter * 1.0);
    st_array[2] = pktin / (inter * 1.0);
    st_array[3] = pktout / (inter * 1.0);
    st_array[4] = pkterrin / (inter * 2.0);
    st_array[5] = pktdrpin / (inter * 2.0);
}

/*  register mod to tsar */
void
mod_register(struct module *mod) {
    register_mod_fields(mod, "--mtraffic" , mtraffic_usage, mtraffic_info, 6, read_mtraffic_stats, set_mtraffic_record);
}

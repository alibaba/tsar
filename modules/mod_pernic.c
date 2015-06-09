#include "tsar.h"

char *pernic_usage = "    --pernic            Net pernic statistics";

#define MAX_NICS 8

/*
 * Structure for pernic infomation.
 */
struct stats_pernic {
    unsigned long long bytein;
    unsigned long long byteout;
    unsigned long long pktin;
    unsigned long long pktout;
    char name[16];
};

#define STATS_TRAFFIC_SIZE (sizeof(struct stats_pernic))


/*
 * collect pernic infomation
 */
static void
read_pernic_stats(struct module *mod)
{
    int                   pos = 0, nics = 0;
    FILE                 *fp;
    char                  line[LEN_1M] = {0};
    char                  buf[LEN_1M] = {0};
    struct stats_pernic   st_pernic;

    memset(buf, 0, LEN_1M);
    memset(&st_pernic, 0, sizeof(struct stats_pernic));

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_1M, fp) != NULL) {
        memset(&st_pernic, 0, sizeof(st_pernic));
        if (!strstr(line, ":")) {
            continue;
        }
        sscanf(line,  "%*[^a-z]%[^:]:%llu %llu %*u %*u %*u %*u %*u %*u "
                "%llu %llu %*u %*u %*u %*u %*u %*u",
                st_pernic.name,
                &st_pernic.bytein,
                &st_pernic.pktin,
                &st_pernic.byteout,
                &st_pernic.pktout);
        /* if nic not used, skip it */
        if (st_pernic.bytein == 0) {
            continue;
        }

        pos += snprintf(buf + pos, LEN_1M - pos, "%s=%lld,%lld,%lld,%lld" ITEM_SPLIT,
                st_pernic.name,
                st_pernic.bytein,
                st_pernic.byteout,
                st_pernic.pktin,
		st_pernic.pktout);
        if (strlen(buf) == LEN_1M - 1) {
            fclose(fp);
            return;
        }

	nics++;
        if (nics > MAX_NICS) {
            break;
        }
    }

    set_mod_record(mod, buf);
    fclose(fp);
    return;
}

static struct mod_info pernic_info[] = {
    {" bytin", SUMMARY_BIT,  MERGE_SUM,  STATS_SUB_INTER},
    {"bytout", SUMMARY_BIT,  MERGE_SUM,  STATS_SUB_INTER},
    {" pktin", DETAIL_BIT,  MERGE_SUM,  STATS_SUB_INTER},
    {"pktout", DETAIL_BIT,  MERGE_SUM,  STATS_SUB_INTER}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pernic", pernic_usage, pernic_info, 4, read_pernic_stats, NULL);
}

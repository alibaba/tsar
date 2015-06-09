#include "tsar.h"

#define UDP_DETAIL_HDR(d)           \
    "  idgm"d"  odgm"d"noport"d"idmerr"

char *udp_usage =
"    --udp               UDP traffic     (v4)";

/* Structure for UDP statistics */
struct stats_udp {
    unsigned long long InDatagrams;
    unsigned long long OutDatagrams;
    unsigned long long NoPorts;
    unsigned long long InErrors;
};

#define STATS_UDP_SIZE      (sizeof(struct stats_udp))

void
read_udp_stats(struct module *mod)
{
    int               sw = FALSE;
    FILE             *fp;
    char              line[LEN_1024];
    char              buf[LEN_1024];
    struct stats_udp  st_udp;

    memset(buf, 0, LEN_1024);
    memset(&st_udp, 0, sizeof(struct stats_udp));
    if ((fp = fopen(NET_SNMP, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_1024, fp) != NULL) {

        if (!strncmp(line, "Udp:", 4)) {
            if (sw) {
                sscanf(line + 4, "%llu %llu %llu %llu",
                        &st_udp.InDatagrams,
                        &st_udp.NoPorts,
                        &st_udp.InErrors,
                        &st_udp.OutDatagrams);
                break;

            } else {
                sw = TRUE;
            }
        }
    }

    fclose(fp);

    int pos = sprintf(buf, "%lld,%lld,%lld,%lld",
            st_udp.InDatagrams,
            st_udp.OutDatagrams,
            st_udp.NoPorts,
            st_udp.InErrors);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

static struct mod_info udp_info[] = {
    {"  idgm", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"  odgm", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"noport", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"idmerr", DETAIL_BIT,  0,  STATS_SUB_INTER},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--udp", udp_usage, udp_info, 4, read_udp_stats, NULL);
}

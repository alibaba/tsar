#include "tsar.h"

char * tcpx_usage="    --tcpx              TCP connection data";

struct stats_tcpx{
    unsigned long long tcprecvq;
    unsigned long long tcpsendq;
    unsigned long long tcpest;
    unsigned long long tcptimewait;
    unsigned long long tcpfinwait1;
    unsigned long long tcpfinwait2;
    unsigned long long tcplistenq;
    unsigned long long tcplistenincq;
    unsigned long long tcplistenover;
    unsigned long long tcpnconnest;
    unsigned long long tcpnconndrop;
    unsigned long long tcpembdrop;
    unsigned long long tcprexmitdrop;
    unsigned long long tcppersistdrop;
    unsigned long long tcpkadrop;
};

static void
read_stat_tcpx(struct module *mod)
{
    int                sw;
    FILE              *fp_tcp;
    FILE              *fp_snmp;
    FILE              *fp_netstat;
    char               buf[LEN_4096], line[LEN_4096];
    struct stats_tcpx  st_tcpx;

    memset(buf, 0, LEN_4096);
    memset(&st_tcpx, 0, sizeof(struct stats_tcpx));

    fp_tcp = fopen(TCP, "r");
    if (fp_tcp == NULL) {
        return;
    }

    fp_snmp = fopen(NET_SNMP, "r");
    if (fp_snmp == NULL) {
        fclose(fp_tcp);
        return;
    }
    fp_netstat = fopen(NETSTAT, "r");
    if (fp_netstat == NULL) {
        fclose(fp_snmp);
        fclose(fp_tcp);
        return;
    }
    st_tcpx.tcplistenq = 0;
    st_tcpx.tcplistenincq = 0;

    sw = 0;
    while (fgets(line, LEN_4096, fp_netstat) !=NULL) {
        if (!strncmp(line, "TcpExt:", 7)) {
            if (!sw) {sw = 1; continue;}
            sscanf(line + 7,
                    "%*u %*u %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %*u %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %llu %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %*u %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %*u %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %*u %*u %*u %*u %*u %*u %*u %*u "
                    "%*u %*u %*u %llu %*u %*u %*u %llu",
                    &st_tcpx.tcplistenover,
                    &st_tcpx.tcpembdrop,
                    &st_tcpx.tcprexmitdrop);
            break;
        }
    }
    sw = 0;
    unsigned long activeopen, passiveopen;
    activeopen = 0;
    passiveopen = 0;
    while (fgets(line, LEN_4096, fp_snmp) !=NULL) {
        if (!strncmp(line, "Tcp:", 4)) {
            if (!sw) {sw = 1; continue;}
            sscanf(line + 4, "%*u %*u %*u %*d %lu %lu",
                    &activeopen, &passiveopen);
            break;
        }
    }

    st_tcpx.tcpnconnest = activeopen + passiveopen;
    st_tcpx.tcpnconndrop = 0;
    st_tcpx.tcppersistdrop = 0;
    st_tcpx.tcpkadrop = 0;

    sprintf(buf,
            "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
            "%llu,%llu,%llu,%llu,%llu,%llu,%llu",
            st_tcpx.tcprecvq,
            st_tcpx.tcpsendq,
            st_tcpx.tcpest,
            st_tcpx.tcptimewait,
            st_tcpx.tcpfinwait1,
            st_tcpx.tcpfinwait2,
            st_tcpx.tcplistenq,
            st_tcpx.tcplistenincq,
            st_tcpx.tcplistenover,
            st_tcpx.tcpnconnest,
            st_tcpx.tcpnconndrop,
            st_tcpx.tcpembdrop,
            st_tcpx.tcprexmitdrop,
            st_tcpx.tcppersistdrop,
            st_tcpx.tcpkadrop);
    set_mod_record(mod, buf);
    fclose(fp_tcp);
    fclose(fp_snmp);
    fclose(fp_netstat);
}

static struct mod_info tcpx_info[]={
    {" recvq", DETAIL_BIT,  0,  STATS_NULL},
    {" sendq", DETAIL_BIT,  0,  STATS_NULL},
    {"   est", DETAIL_BIT,  0,  STATS_NULL},
    {" twait", DETAIL_BIT,  0,  STATS_NULL},
    {"fwait1", DETAIL_BIT,  0,  STATS_NULL},
    {"fwait2", DETAIL_BIT,  0,  STATS_NULL},
    {"  lisq", DETAIL_BIT,  0,  STATS_NULL},
    {"lising", DETAIL_BIT,  0,  STATS_NULL},
    {"lisove", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" cnest", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" ndrop", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" edrop", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" rdrop", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" pdrop", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {" kdrop", DETAIL_BIT,  0,  STATS_SUB_INTER},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--tcpx", tcpx_usage, tcpx_info, 15, read_stat_tcpx, NULL);
}

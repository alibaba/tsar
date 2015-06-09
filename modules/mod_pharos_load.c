#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


#define sub(a, b) ((a) <= (b) ? 0 : (a) - (b))

struct stats_load_pharos {
    unsigned long long total_reloads;
    unsigned long long success_reloads;
    unsigned long long sn;
    unsigned long long rip;
    unsigned long long wrp;
    unsigned long long wideip_pool;
    unsigned long long fpool;
    unsigned long long introduce_domain;
    unsigned long long soa;
    unsigned long long ns;
    unsigned long long wideip;
    unsigned long long region;
    unsigned long long pool;
    char               ignore1[16];
    char               ignore2[16];
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *pharos_load_usage = "    --pharos_load      pharos load statistics";

static struct mod_info pharos_load_info[] = {
    {"loads",      DETAIL_BIT,  0,  STATS_NULL},
    {"sloads",     DETAIL_BIT,  0,  STATS_NULL},
    {"sn",         DETAIL_BIT,  0,  STATS_NULL},
    {"reg_ip",     DETAIL_BIT,  0,  STATS_NULL},
    {"wrp",        DETAIL_BIT,  0,  STATS_NULL},
    {"w_pool",     DETAIL_BIT,  0,  STATS_NULL},
    {"pool",       DETAIL_BIT,  0,  STATS_NULL},
    {"intr_d",     DETAIL_BIT,  0,  STATS_NULL},
    {"soa",        DETAIL_BIT,  0,  STATS_NULL},
    {"ns",         DETAIL_BIT,  0,  STATS_NULL},
    {"wideip",     DETAIL_BIT,  0,  STATS_NULL},
    {"region",     DETAIL_BIT,  0,  STATS_NULL},
    {"pool",       DETAIL_BIT,  0,  STATS_NULL},
    {"load_t",     DETAIL_BIT,  0,  STATS_NULL}
};


static void
set_pharos_load_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[0];
    st_array[1] = cur_array[1];
    st_array[2] = cur_array[2];
    st_array[3] = cur_array[3];
    st_array[4] = cur_array[4];
    st_array[5] = cur_array[5];
    st_array[6] = cur_array[6];
    st_array[7] = cur_array[7];
    st_array[8] = cur_array[8];
    st_array[9] = cur_array[9];
    st_array[10] = cur_array[10];
    st_array[11] = cur_array[11];
    st_array[12] = cur_array[12];
}


static void
init_pharos_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("PHAROS_TSAR_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("PHAROS_TSAR_PORT");
    p->port = port ? atoi(port) : 8080;

    p->uri = getenv("PHAROS_TSAR_URI");
    p->uri = p->uri ? p->uri : "/pharos_status";

    p->server_name = getenv("PHAROS_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "status.taobao.com";
}


void
read_pharos_load_stats(struct module *mod, char *parameter)
{
    int                 write_flag = 0, addr_len, domain;
    int                 m, sockfd, send, pos;
    void               *addr;
    char                buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    FILE               *stream = NULL;

    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;

    init_pharos_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_load_pharos st_pharos;
    memset(&st_pharos, 0, sizeof(struct stats_load_pharos));

    if (*hinfo.host == '/') {
        addr = &servaddr_un;
        addr_len = sizeof(servaddr_un);
        bzero(addr, addr_len);
        domain = AF_LOCAL;
        servaddr_un.sun_family = AF_LOCAL;
        strncpy(servaddr_un.sun_path, hinfo.host, sizeof(servaddr_un.sun_path) - 1);

    } else {
        addr = &servaddr;
        addr_len = sizeof(servaddr);
        bzero(addr, addr_len);
        domain = AF_INET;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(hinfo.port);
        inet_pton(AF_INET, hinfo.host, &servaddr.sin_addr);
    }


    if ((sockfd = socket(domain, SOCK_STREAM, 0)) == -1) {
        goto writebuf;
    }

    sprintf(request,
            "GET %s HTTP/1.0\r\n"
            "User-Agent: taobot\r\n"
            "Host: %s\r\n"
            "Accept:*/*\r\n"
            "Connection: Close\r\n\r\n",
            hinfo.uri, hinfo.server_name);

    if ((m = connect(sockfd, (struct sockaddr *) addr, addr_len)) == -1 ) {
        goto writebuf;
    }

    if ((send = write(sockfd, request, strlen(request))) == -1) {
        goto writebuf;
    }

    if ((stream = fdopen(sockfd, "r")) == NULL) {
        goto writebuf;
    }

    while (fgets(line, LEN_4096, stream) != NULL) {
        if (!strncmp(line, "reload_status:", sizeof("reload_status:") - 1)) {
            sscanf(line, "reload_status:total_reload_num=%llu,success_reload_num=%llu",
                    &st_pharos.total_reloads, &st_pharos.success_reloads);
            write_flag = 1;
        }

        if (!strncmp(line, "mysql_module_status:", sizeof("mysql_module_status:") - 1)) {
            sscanf(line, "mysql_module_status:sn=%llu,latest_reload_time=%s %8s,frs_region_ip=%llu,frs_wrp=%llu,frs_wideip_pool=%llu,frs_pool=%llu,introduce_domain=%llu,soa=%llu,sn=%llu,wideip=%llu,region=%llu,pool=%llu",
                    &st_pharos.sn,
                    st_pharos.ignore1,
                    st_pharos.ignore2,
                    &st_pharos.rip,
                    &st_pharos.wrp,
                    &st_pharos.wideip_pool,
                    &st_pharos.fpool,
                    &st_pharos.introduce_domain,
                    &st_pharos.soa,
                    &st_pharos.ns,
                    &st_pharos.wideip,
                    &st_pharos.region,
                    &st_pharos.pool);
            write_flag = 1;
        }
    }

writebuf:
    if (stream) {
        fclose(stream);
    }

    if (sockfd != -1) {
        close(sockfd);
    }

    if (write_flag) {
        pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                st_pharos.total_reloads,
                st_pharos.success_reloads,
                st_pharos.sn,
                st_pharos.rip,
                st_pharos.wrp,
                st_pharos.wideip_pool,
                st_pharos.fpool,
                st_pharos.introduce_domain,
                st_pharos.soa,
                st_pharos.ns,
                st_pharos.wideip,
                st_pharos.region,
                st_pharos.pool);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pharos_load", pharos_load_usage, pharos_load_info,
                        13, read_pharos_load_stats, set_pharos_load_record);
}

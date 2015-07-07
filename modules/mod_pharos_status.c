#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


#define sub(a, b) ((a) <= (b) ? 0 : (a) - (b))

struct stats_pharos_status {
    unsigned long long total;

    unsigned long long noerror;
    unsigned long long formerr;
    unsigned long long servfail;
    unsigned long long nxdomain;
    unsigned long long notimp;
    unsigned long long refused;
    unsigned long long global;
    unsigned long long continent;
    unsigned long long country;
    unsigned long long isp;
    unsigned long long area;
    unsigned long long province;
    unsigned long long city;
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *pharos_status_usage = "    --pharos-status            Pharos statistics";

static struct mod_info pharos_status_info[] = {
    {"noerr",       DETAIL_BIT,  0, STATS_NULL},   // 0
    {"fmerr",       DETAIL_BIT,  0, STATS_NULL},   // 1
    {"svfail",      DETAIL_BIT,  0, STATS_NULL},   // 2
    {"nx",          DETAIL_BIT,  0, STATS_NULL},   // 3
    {"refuse",      DETAIL_BIT,  0, STATS_NULL},   // 4
    {"global",      SUMMARY_BIT, 0, STATS_NULL},   // 5
    {"contnt",      SUMMARY_BIT, 0, STATS_NULL},   // 6
    {"coutry",      SUMMARY_BIT, 0, STATS_NULL},   // 7
    {"isp",         SUMMARY_BIT, 0, STATS_NULL},   // 8
    {"area",        SUMMARY_BIT, 0, STATS_NULL},   // 9
    {"provic",      SUMMARY_BIT, 0, STATS_NULL},   // 10
    {"city",        SUMMARY_BIT, 0, STATS_NULL}    // 11
};


static void
set_pharos_status_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    double t;

    st_array[12] = sub(cur_array[12], pre_array[12]);
    t = st_array[12];

    if (t == 0) {
        t = 1;
    }

    st_array[0] = sub(cur_array[0], pre_array[0]);                      // noerr
    st_array[1] = sub(cur_array[1], pre_array[1]);                      // formerr
    st_array[2] = sub(cur_array[2], pre_array[2]);                      // srvfail
    st_array[3] = sub(cur_array[3], pre_array[2]);                      // nx
    st_array[4] = sub(cur_array[4], pre_array[4]);                      // refused
    st_array[5] = sub(cur_array[5], pre_array[5]) * 1.0 / t * 100;      // global
    st_array[6] = sub(cur_array[6], pre_array[6]) * 1.0 / t * 100;      // continent
    st_array[7] = sub(cur_array[7], pre_array[7]) * 1.0 / t * 100;      // country
    st_array[8] = sub(cur_array[8], pre_array[8]) * 1.0 / t * 100;      // isp
    st_array[9] = sub(cur_array[9], pre_array[9]) * 1.0 / t * 100;      // area
    st_array[10] = sub(cur_array[10], pre_array[10]) * 1.0 / t * 100;   // province
    st_array[11] = sub(cur_array[11], pre_array[11]) * 1.0 / t * 100;   // city
}


static void
init_pharos_status_host_info(struct hostinfo *p)
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
read_pharos_status_stats(struct module *mod, char *parameter)
{
    int                 write_flag = 0, addr_len, domain;
    int                 m, sockfd, send, pos;
    void               *addr;
    char                buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    FILE               *stream = NULL;

    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;

    init_pharos_status_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_pharos_status st_pharos;
    memset(&st_pharos, 0, sizeof(struct stats_pharos_status));

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
        // read retcode
        if (!strncmp(line, "retcode:", sizeof("retcode:") - 1)) {
            sscanf(line, "retcode:NOERROR=%llu,FORMERR=%llu,SERVFAIL=%llu,NXDOMAIN=%llu,NOTIMP=%llu,REFUSED=%llu",
                    &st_pharos.noerror,
                    &st_pharos.formerr,
                    &st_pharos.servfail,
                    &st_pharos.nxdomain,
                    &st_pharos.notimp,
                    &st_pharos.refused);

            write_flag = 1;
        }

        // read hits
        if (!strncmp(line, "hits:", sizeof("hits:") - 1)) {
            sscanf(line, "hits:global=%llu,continent=%llu,country=%llu,isp=%llu,area=%llu,province=%llu,city=%llu",
                    &st_pharos.global,
                    &st_pharos.continent,
                    &st_pharos.country,
                    &st_pharos.isp,
                    &st_pharos.area,
                    &st_pharos.province,
                    &st_pharos.city);

            write_flag = 1;
        }

        // read requests
        if (!strncmp(line, "request_status:", sizeof("request_status:") - 1)) {
            sscanf(line, "request_status:requests=%llu", &st_pharos.total);

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
                st_pharos.noerror,
                st_pharos.formerr,
                st_pharos.servfail,
                st_pharos.nxdomain,
                st_pharos.refused,
                st_pharos.global,
                st_pharos.continent,
                st_pharos.country,
                st_pharos.isp,
                st_pharos.area,
                st_pharos.province,
                st_pharos.city,
                st_pharos.total);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pharos-status",
                        pharos_status_usage,
                        pharos_status_info,
                        13,
                        read_pharos_status_stats,
                        set_pharos_status_record);
}

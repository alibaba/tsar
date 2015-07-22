#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


#define sub(a, b) ((a) <= (b) ? 0 : (a) - (b))

struct stats_pharos_rcode {
    unsigned long long total;
    unsigned long long noerror;
    unsigned long long formerr;
    unsigned long long servfail;
    unsigned long long nxdomain;
    unsigned long long notimp;
    unsigned long long refused;
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *pharos_rcode_usage = "    --pharos-rcode            Pharos rcode statistics";

static struct mod_info pharos_rcode_info[] = {
    {" total",    DETAIL_BIT,  0, STATS_NULL},
    {" noerr",    DETAIL_BIT,  0, STATS_NULL},
    {"  nxdm",    DETAIL_BIT,  0, STATS_NULL},
    {"refuse",    DETAIL_BIT,  0, STATS_NULL},
    {"fmterr",    DETAIL_BIT,  0, STATS_NULL},
    {"svfail",    DETAIL_BIT,  0, STATS_NULL},
    {"notimp",    DETAIL_BIT,  0, STATS_NULL},
};


static void
set_pharos_rcode_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = sub(cur_array[0], pre_array[0]); // noerror
    st_array[1] = sub(cur_array[1], pre_array[1]); // nxdomain
    st_array[2] = sub(cur_array[2], pre_array[2]); // refused
    st_array[3] = sub(cur_array[3], pre_array[3]); // formaterror
    st_array[4] = sub(cur_array[4], pre_array[4]); // servfail
    st_array[5] = sub(cur_array[5], pre_array[5]); // notimp
    st_array[6] = sub(cur_array[6], pre_array[6]); // total
}

static void
init_pharos_rcode_host_info(struct hostinfo *p)
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
read_pharos_rcode_stats(struct module *mod, char *parameter)
{
    int                 write_flag = 0, addr_len, domain;
    int                 m, sockfd, send, pos;
    void               *addr;
    char                buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    FILE               *stream = NULL;

    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;

    init_pharos_rcode_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_pharos_rcode st_pharos_rcode;
    memset(&st_pharos_rcode, 0, sizeof(struct stats_pharos_rcode));

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
                    &st_pharos_rcode.noerror,
                    &st_pharos_rcode.formerr,
                    &st_pharos_rcode.servfail,
                    &st_pharos_rcode.nxdomain,
                    &st_pharos_rcode.notimp,
                    &st_pharos_rcode.refused);

            write_flag = 1;
        }

        // read requests
        if (!strncmp(line, "request_status:", sizeof("request_status:") - 1)) {
            sscanf(line, "request_status:requests=%llu", &st_pharos_rcode.total);

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
        pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                      st_pharos_rcode.total,
                      st_pharos_rcode.noerror,
                      st_pharos_rcode.nxdomain,
                      st_pharos_rcode.refused,
                      st_pharos_rcode.formerr,
                      st_pharos_rcode.servfail,
                      st_pharos_rcode.notimp);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pharos-rcode", pharos_rcode_usage, pharos_rcode_info, 7,
                        read_pharos_rcode_stats, set_pharos_rcode_record);
}

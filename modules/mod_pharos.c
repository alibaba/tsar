#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


#define sub(a, b) ((a) <= (b) ? 0 : (a) - (b))

struct stats_pharos {
    unsigned long long requests;
    unsigned long long tcp_reqs;
    unsigned long long udp_reqs;
    unsigned long long tcp_accepts;
    unsigned long long rt;
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *pharos_usage = "    --pharos            Pharos statistics";

static struct mod_info pharos_info[] = {
    {"reqs",   DETAIL_BIT,  0,  STATS_NULL},
    {"tcp",    DETAIL_BIT,  0,  STATS_NULL},
    {"udp",    DETAIL_BIT,  0,  STATS_NULL},
    {"qps",    SUMMARY_BIT, 0,  STATS_NULL},
    {"rt",     DETAIL_BIT,  0,  STATS_NULL}
};


static void
set_pharos_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = sub(cur_array[0], pre_array[0]);
    st_array[1] = sub(cur_array[1], pre_array[1]);
    st_array[2] = sub(cur_array[2], pre_array[2]);
    st_array[3] = st_array[0] * 1.0 / inter;
    st_array[4] = sub(cur_array[3], pre_array[3]) * 1.0 / st_array[0];
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
read_pharos_stats(struct module *mod, char *parameter)
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
    struct stats_pharos st_pharos;
    memset(&st_pharos, 0, sizeof(struct stats_pharos));

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
        if (!strncmp(line, "request_status:", sizeof("request_status:") - 1)) {
            sscanf(line, "request_status:requests=%llu,tcp_reqs=%llu,udp_reqs=%llu,tcp_accepts=%llu,rt=%llu",
                    &st_pharos.requests, &st_pharos.tcp_reqs, &st_pharos.udp_reqs,
                    &st_pharos.tcp_accepts, &st_pharos.rt);
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
        pos = sprintf(buf, "%lld,%lld,%lld,%lld,",
                st_pharos.requests,
                st_pharos.tcp_reqs,
                st_pharos.udp_reqs,
                st_pharos.rt);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pharos", pharos_usage, pharos_info, 5, read_pharos_stats, set_pharos_record);
}

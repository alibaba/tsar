#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


struct stats_nginx_code {
    unsigned long long n2XX;        /* 2XX status code */
    unsigned long long n3XX;        /* 3XX status code */
    unsigned long long n4XX;        /* 4XX status code */
    unsigned long long n5XX;        /* 5XX status code */
    unsigned long long other;       /* other status code */
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_code_usage = "    --nginx_code        nginx httpcode";

static struct mod_info nginx_code_info[] = {
    {"   2XX", DETAIL_BIT, 0,  STATS_NULL},
    {"   3XX", DETAIL_BIT, 0,  STATS_NULL},
    {"   4XX", DETAIL_BIT, 0,  STATS_NULL},
    {"   5XX", DETAIL_BIT, 0,  STATS_NULL},
    {" other", DETAIL_BIT, 0,  STATS_NULL},
};


static void
set_nginx_code_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        }
    }
}


static void
init_nginx_code_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("NGX_TSAR_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("NGX_TSAR_PORT");
    p->port = port ? atoi(port) : 80;

    p->uri = getenv("NGX_TSAR_DOMAIN_URI");
    p->uri = p->uri ? p->uri : "/traffic_status";

    p->server_name = getenv("NGX_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "status.taobao.com";
}


void
read_nginx_code_stats(struct module *mod, char *parameter)
{
    int                      addr_len, domain, m, sockfd, send, pos = 0;
    char                     buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    char                    *p;
    void                    *addr;
    FILE                    *stream = NULL;
    struct hostinfo          hinfo;
    struct sockaddr_in       servaddr;
    struct sockaddr_un       servaddr_un;
    struct stats_nginx_code  cur_stats, total_stats;

    /* get peer info */
    init_nginx_code_host_info(&hinfo);
    if (parameter && atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }

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

    /* send request */
    if ((sockfd = socket(domain, SOCK_STREAM, 0)) == -1) {
        return;
    }

    sprintf(request,
            "GET %s HTTP/1.0\r\n"
            "User-Agent: taobot\r\n"
            "Host: %s\r\n"
            "Accept:*/*\r\n"
            "Connection: Close\r\n\r\n",
            hinfo.uri, hinfo.server_name);

    if ((m = connect(sockfd, (struct sockaddr *) addr, addr_len)) == -1 ) {
        return;
    }

    if ((send = write(sockfd, request, strlen(request))) == -1) {
        return;
    }

    /* read & parse request */
    if ((stream = fdopen(sockfd, "r")) == NULL) {
        close(sockfd);
        return;
    }

    memset(&total_stats, 0, sizeof(total_stats));
    while (fgets(line, LEN_4096, stream) != NULL) {
        memset(&cur_stats, 0, sizeof(cur_stats));
        if ((p = strchr(line, ',')) == NULL) {
            continue;
        }
	*p++ = '\0';
        if (sscanf(p, "%*u,%*u,%*u,%*u,%llu,%llu,%llu,%llu,%llu,%*u,%*u,%*u,%*u",
                   &cur_stats.n2XX, &cur_stats.n3XX, &cur_stats.n4XX, &cur_stats.n5XX, &cur_stats.other) != 5) {
            continue;
        }

        total_stats.n2XX += cur_stats.n2XX;
        total_stats.n3XX += cur_stats.n3XX;
        total_stats.n4XX += cur_stats.n4XX;
        total_stats.n5XX += cur_stats.n5XX;
        total_stats.other += cur_stats.other;
    }

    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld",
        total_stats.n2XX,
        total_stats.n3XX,
        total_stats.n4XX,
        total_stats.n5XX,
        total_stats.other);

    buf[pos] = '\0';
    set_mod_record(mod, buf);

    fclose(stream);
    close(sockfd);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--nginx_code", nginx_code_usage, nginx_code_info, 5, read_nginx_code_stats, set_nginx_code_record);
}

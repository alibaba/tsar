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

    unsigned long long n200;        /* 200 status code */
    unsigned long long n206;        /* 206 status code */
    unsigned long long n302;        /* 302 status code */
    unsigned long long n304;        /* 304 status code */
    unsigned long long n403;        /* 403 status code */
    unsigned long long n404;        /* 404 status code */
    unsigned long long n416;        /* 416 status code */
    unsigned long long n499;        /* 499 status code */
    unsigned long long n500;        /* 500 status code */
    unsigned long long n502;        /* 502 status code */
    unsigned long long n503;        /* 503 status code */
    unsigned long long n504;        /* 504 status code */
    unsigned long long n508;        /* 508 status code */
    unsigned long long detail_other;    /* status other than above 13 detail code*/
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_code_usage = "    --nginx_code        nginx httpcode";

static struct mod_info nginx_code_info[] = {
    {"   2XX", HIDE_BIT, 0,  STATS_NULL},
    {"   3XX", HIDE_BIT, 0,  STATS_NULL},
    {"   4XX", HIDE_BIT, 0,  STATS_NULL},
    {"   5XX", HIDE_BIT, 0,  STATS_NULL},
    {" other", HIDE_BIT, 0,  STATS_NULL},

    {"   200", DETAIL_BIT, 0,  STATS_NULL},
    {"   206", DETAIL_BIT, 0,  STATS_NULL},
    {"   302", DETAIL_BIT, 0,  STATS_NULL},
    {"   304", DETAIL_BIT, 0,  STATS_NULL},
    {"   403", DETAIL_BIT, 0,  STATS_NULL},
    {"   404", DETAIL_BIT, 0,  STATS_NULL},
    {"   416", DETAIL_BIT, 0,  STATS_NULL},
    {"   499", DETAIL_BIT, 0,  STATS_NULL},
    {"   500", DETAIL_BIT, 0,  STATS_NULL},
    {"   502", DETAIL_BIT, 0,  STATS_NULL},
    {"   503", DETAIL_BIT, 0,  STATS_NULL},
    {"   504", DETAIL_BIT, 0,  STATS_NULL},
    {"   508", DETAIL_BIT, 0,  STATS_NULL},
    {"dother", DETAIL_BIT, 0,  STATS_NULL},
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
        close(sockfd);
        return;
    }

    if ((send = write(sockfd, request, strlen(request))) == -1) {
        close(sockfd);
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

        if (sscanf(p,
                   "%*u,%*u,%*u,%*u,%llu,%llu,%llu,%llu,%llu,%*u,%*u,%*u,%*u,"
                   "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                   &cur_stats.n2XX, &cur_stats.n3XX, &cur_stats.n4XX, &cur_stats.n5XX, &cur_stats.other,
                   &cur_stats.n200, &cur_stats.n206, &cur_stats.n302, &cur_stats.n304, &cur_stats.n403,
                   &cur_stats.n404, &cur_stats.n416, &cur_stats.n499, &cur_stats.n500, &cur_stats.n502,
                   &cur_stats.n503, &cur_stats.n504, &cur_stats.n508, &cur_stats.detail_other)
            != 19) {
            continue;
        }

        total_stats.n2XX += cur_stats.n2XX;
        total_stats.n3XX += cur_stats.n3XX;
        total_stats.n4XX += cur_stats.n4XX;
        total_stats.n5XX += cur_stats.n5XX;
        total_stats.other += cur_stats.other;

        total_stats.n200 += cur_stats.n200;
        total_stats.n206 += cur_stats.n206;
        total_stats.n302 += cur_stats.n302;
        total_stats.n304 += cur_stats.n304;
        total_stats.n403 += cur_stats.n403;
        total_stats.n404 += cur_stats.n404;
        total_stats.n416 += cur_stats.n416;
        total_stats.n499 += cur_stats.n499;
        total_stats.n500 += cur_stats.n500;
        total_stats.n502 += cur_stats.n502;
        total_stats.n503 += cur_stats.n503;
        total_stats.n504 += cur_stats.n504;
        total_stats.n508 += cur_stats.n508;
        total_stats.detail_other += cur_stats.detail_other;
    }

    pos = sprintf(buf,
                  "%lld,%lld,%lld,%lld,%lld,"
                  "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                  "%lld,%lld,%lld,%lld",
                  total_stats.n2XX,
                  total_stats.n3XX,
                  total_stats.n4XX,
                  total_stats.n5XX,
                  total_stats.other,

                  total_stats.n200,
                  total_stats.n206,
                  total_stats.n302,
                  total_stats.n304,
                  total_stats.n403,
                  total_stats.n404,
                  total_stats.n416,
                  total_stats.n499,
                  total_stats.n500,
                  total_stats.n502,
                  total_stats.n503,
                  total_stats.n504,
                  total_stats.n508,
                  total_stats.detail_other);

    buf[pos] = '\0';
    set_mod_record(mod, buf);

    fclose(stream);
    close(sockfd);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--nginx_code", nginx_code_usage, nginx_code_info, 19, read_nginx_code_stats, set_nginx_code_record);
}

#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"
#include <stdio.h>

#define NUM_DOMAIN_MAX 64
#define MAX 40960
#define DOMAIN_LIST_DELIM ", \t"

struct stats_nginx_domain {
    unsigned long long traffic;     /* upstream response bytes */
    unsigned long long reqs;        /* upstream requests */
    unsigned long long n4XX;        /* upstream 4XX status code */
    unsigned long long n5XX;        /* upstream 5XX status code */
    unsigned long long tries;       /* upstream tries requests */
    unsigned long long rt;          /* upstream response time sum of total requests */
    unsigned long long fbt;         /* response first byte time sum of total requests */
    unsigned long long ufbt;        /* upstream response first byte time sum of total requests */
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_ups_usage = "    --nginx_ups      nginx upstream statistics";

static struct mod_info nginx_info[] = {
    {" traff", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   qps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  rqps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   4XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   5XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"    rt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   fbt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  ufbt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
};


static void
set_nginx_ups_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < mod->n_col; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        } else {
            st_array[i] = 0;
        }
    }
}


static void
init_nginx_host_info(struct hostinfo *p)
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


static void
read_nginx_domain_ups_stats(struct module *mod, char *parameter)
{
    int                 i, addr_len, domain, m, sockfd, send, pos = 0;
    char                buf[LEN_1M], request[LEN_4096], line[LEN_4096];
    char               *p;
    void               *addr;
    FILE               *stream = NULL;
    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;
    struct stats_nginx_domain  total_stat, stat;
    unsigned long long  d;

    /* get peer info */
    init_nginx_host_info(&hinfo);

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

    memset(&total_stat, 0, sizeof(struct stats_nginx_domain));

    while (fgets(line, LEN_4096, stream) != NULL) {
        if ((p = strchr(line, ',')) == NULL) {
            continue;
        }
        *p++ = '\0';    /* stat.domain terminating null */

        memset(&stat, 0, sizeof(struct stats_nginx_domain));
        if (sscanf(p,
                   "%llu,%llu,%llu,%llu,"               /* 4 */
                   "%llu,%llu,%llu,%llu,%llu,%llu,"     /* 4 + 6 = 10 */
                   "%llu,%llu,%llu,",                   /* 10 + 3 = 13: ureq, urt, utries */
                   &d, &d, &d, &d, &d, &d, &d, &d, &d, &d,
                   &stat.reqs, &stat.rt, &stat.tries)
            != 13)
        {
            continue;
        }
        /* skip 27 fields */
        for (i = 0; *p != '\0'; p++) {
            if (*p == ',') {
                i++;
            }
            if (i == 27) {
                break;
            }
        }
        if (i != 27) {
            continue;
        }

        p++;    /* skip `,' */
        /* up4xx, up5xx, fbt, ufbt, ups_response_length */
        if (sscanf(p, "%llu,%llu,%llu,%llu,%llu",
                      &stat.n4XX, &stat.n5XX, &stat.fbt, &stat.ufbt, &stat.traffic)
            != 5)
        {
            continue;
        }
        /* sum */
        total_stat.traffic += stat.traffic;
        total_stat.reqs += stat.reqs;
        total_stat.n4XX += stat.n4XX;
        total_stat.n5XX += stat.n5XX;
        total_stat.tries += stat.tries;
        total_stat.rt += stat.rt;
        total_stat.fbt += stat.fbt;
        total_stat.ufbt += stat.ufbt;
    }

    pos = sprintf(buf,
                  "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                  total_stat.traffic, total_stat.reqs, total_stat.tries, total_stat.n4XX, total_stat.n5XX,
                  total_stat.rt, total_stat.fbt, total_stat.ufbt);

    buf[pos] = '\0';

    set_mod_record(mod, buf);
    fclose(stream);
    close(sockfd);
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--nginx_ups", nginx_ups_usage, nginx_info, 8,
                        read_nginx_domain_ups_stats, set_nginx_ups_record);
}

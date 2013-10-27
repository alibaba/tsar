#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


struct stats_nginx {
    char *domain;                   /* domain name */
    unsigned long long nbytesin;    /* total bytes in */
    unsigned long long nbytesout;   /* total bytes out */
    unsigned long long nconn;       /* total connections */
    unsigned long long nreq;        /* total requests */
    unsigned long long n2XX;        /* 2XX status code */
    unsigned long long n3XX;        /* 3XX status code */
    unsigned long long n4XX;        /* 4XX status code */
    unsigned long long n5XX;        /* 5XX status code */
    unsigned long long nother;      /* other status code & http version 0.9 responses */
    unsigned long long rt;          /* response time sum of total requests */
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_usage = "    --nginx_domain      nginx domain statistics";

static struct mod_info nginx_info[] = {
    {"   cps", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"   qps", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"   2XX", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"   3XX", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"   4XX", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"   5XX", SUMMARY_BIT, 0,  STATS_SUB_INTER},
    {"    rt", SUMMARY_BIT, 0,  STATS_NULL},
};


static void
set_nginx_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    for (i = 0; i < 6; i++) {
        st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
    }

    /* avg_rt = (cur_rt - pre_rt) / (cur_nreq - pre_nreq) */
    if (cur_array[6] >= pre_array[6] && cur_array[1] > pre_array[1]) {
        st_array[6] = (cur_array[6] - pre_array[6]) * 1.0 / (cur_array[1] - pre_array[1]);
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
    p->uri = p->uri ? p->uri : "/nginx_domain_status";

    p->server_name = getenv("NGX_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "status.taobao.com";
}


void
read_nginx_domain_stats(struct module *mod, char *parameter)
{
    int                 addr_len, domain, m, sockfd, send, pos = 0;
    char                buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    char               *p;
    void               *addr;
    FILE               *stream = NULL;
    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;
    struct stats_nginx  stat;

    /* get peer info */
    init_nginx_host_info(&hinfo);
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

    while (fgets(line, LEN_4096, stream) != NULL) {
        if ((p = strchr(line, ',')) == NULL) {
            continue;
        }
        *p++ = '\0';    /* stat.domain terminating null */

        if (sscanf(p, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                   &stat.nbytesin, &stat.nbytesout, &stat.nconn, &stat.nreq,
                   &stat.n2XX, &stat.n3XX, &stat.n4XX, &stat.n5XX, &stat.nother, &stat.rt) != 10) {
            continue;
        }
        stat.domain = line;

        pos += sprintf(buf + pos, "%s=%lld,%lld,%lld,%lld,%lld,%lld,%lld" ITEM_SPLIT,
                stat.domain, stat.nconn, stat.nreq, stat.n2XX, stat.n3XX, stat.n4XX, stat.n5XX, stat.rt);
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);

    fclose(stream);
    close(sockfd);
}

void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--nginx_domain", nginx_usage, nginx_info, 7, read_nginx_domain_stats, set_nginx_record);
}

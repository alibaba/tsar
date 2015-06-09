#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"
#include <stdio.h>

#define NUM_DOMAIN_MAX 64
#define MAX 1024
#define DOMAIN_LIST_DELIM ", \t"

int nginx_port = 80;
int domain_num = 0;
int top_domain = 0;
int all_domain = 0;

struct stats_nginx_domain {
    char domain[LEN_4096];               /* domain name */
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
    unsigned long long uprt;        /* response time sum of total upstream requests */
    unsigned long long upreq;       /* total upstream request */
    unsigned long long upactreq;    /* actual upstream requests */
};

/* struct for http domain */
static struct stats_nginx_domain nginx_domain_stats[MAX];
static int nginxcmp(const void *a, const void *b)
{
    struct stats_nginx_domain *pa = (struct stats_nginx_domain *)a, *pb = (struct stats_nginx_domain *)b;
    return strcmp(pa->domain, pb->domain);
}

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_domain_usage = "    --nginx_domain      nginx domain statistics";

static struct mod_info nginx_info[] = {
    {"   cps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   qps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   2XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   3XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   4XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"   5XX", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"    rt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"  uprt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" upret", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" upqps", HIDE_BIT, MERGE_SUM,  STATS_NULL}
};

static void nginx_domain_init(char *parameter)
{
    FILE    *fp;
    char    *line = NULL, *domain, *token;
    size_t   size = LEN_1024;
    ssize_t  ret = 0;

    domain_num = 0;
    memset(nginx_domain_stats, 0, MAX * sizeof(struct stats_nginx_domain));

    fp = fopen(parameter, "r");
    if (fp == NULL) {
        nginx_port = 80;
        all_domain = 1;
        return;
    }

    while ((ret = getline(&line, &size, fp)) > 0) {
        if (ret > 5 && strncasecmp("port=", line, 5) == 0) {
            nginx_port = atoi(line + 5);
            if (!nginx_port) {
                nginx_port = 80;
            }
        } else if (ret > 4 && strncasecmp("top=", line, 4) == 0) {
            top_domain = atoi(line + 4);
            if (!top_domain) {
                top_domain = NUM_DOMAIN_MAX;
            }
        } else if (ret > 7 && strncasecmp("domain=", line, 7) == 0) {
            line[ret - 1] = '\0';
            domain = line + 7;
            token = strtok(domain, DOMAIN_LIST_DELIM);
            while (token != NULL) {
                if (domain_num >= NUM_DOMAIN_MAX) {
                    continue;
                }
                strcpy(nginx_domain_stats[domain_num].domain, token);
                domain_num++;
                token = strtok(NULL, DOMAIN_LIST_DELIM);
            }
        }
    }
    if (top_domain > domain_num && domain_num > 0) {
        top_domain = domain_num;
    }
    if (domain_num == 0) {
        all_domain = 1;
    }

    qsort(nginx_domain_stats, domain_num, sizeof(nginx_domain_stats[0]), nginxcmp);
    if (line != NULL) {
        free(line);
    }
}

static void
set_nginx_domain_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        } else {
            st_array[i] = 0;
        }
    }

    /* avg_rt = (cur_rt - pre_rt) / (cur_nreq - pre_nreq) */
    if (cur_array[6] >= pre_array[6] && cur_array[1] > pre_array[1]) {
        st_array[6] = (cur_array[6] - pre_array[6]) * 1.0 / (cur_array[1] - pre_array[1]);
    }

    /* upstream request rt */
    if (cur_array[7] >= pre_array[7] && cur_array[9] > pre_array[9]) {
        st_array[7] = (cur_array[7] - pre_array[7]) * 1.0 / (cur_array[9] - pre_array[9]);
    }

    /* upstream retry percent = (actual upstream request - total upstream request)/total upstream request */
    if (cur_array[8] >= pre_array[8] && cur_array[9] > pre_array[9] && (cur_array[9] - pre_array[9]) >= (cur_array[8] - pre_array[8])) {
        st_array[8] = ((cur_array[9] - pre_array[9]) - (cur_array[8] - pre_array[8])) * 1.0 / (cur_array[9] - pre_array[9]);
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


void
read_nginx_domain_stats(struct module *mod, char *parameter)
{
    int                 i, addr_len, domain, m, sockfd, send, pos = 0;
    char                buf[LEN_1M], request[LEN_4096], line[LEN_4096];
    char               *p;
    void               *addr;
    FILE               *stream = NULL;
    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;
    struct stats_nginx_domain  stat;
    struct stats_nginx_domain *pair;

    /* get peer info */
    init_nginx_host_info(&hinfo);

    nginx_domain_init(parameter);

    hinfo.port = nginx_port;

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

    while (fgets(line, LEN_4096, stream) != NULL) {
        if ((p = strchr(line, ',')) == NULL) {
            continue;
        }
        *p++ = '\0';    /* stat.domain terminating null */

        memset(&stat, 0, sizeof(struct stats_nginx_domain));
        if (sscanf(p, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
                   "%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u,%*u",
                   &stat.nbytesin, &stat.nbytesout, &stat.nconn, &stat.nreq, &stat.n2XX, &stat.n3XX, &stat.n4XX, &stat.n5XX, &stat.nother, &stat.rt, &stat.upreq, &stat.uprt, &stat.upactreq) != 13) {
            continue;
        }
        strcpy(stat.domain, line);
        if(strlen(stat.domain) == 0) {
            strcpy(stat.domain, "null");
        }

        if (all_domain == 0) {
            pair = bsearch(&stat, nginx_domain_stats, domain_num, sizeof(nginx_domain_stats[0]), nginxcmp);
            if (pair == NULL) {
                continue;
	    } else {
	        memcpy(pair, &stat, sizeof(struct stats_nginx_domain));
	    }
	} else {
            if(domain_num >= MAX) {
                continue;
            }
            memcpy(&nginx_domain_stats[domain_num], &stat, sizeof(struct stats_nginx_domain));
            domain_num++;
        }
    }
    if (top_domain == 0 || top_domain > domain_num) {
        top_domain = domain_num;
    }
    if (top_domain > MAX) {
        top_domain = MAX;
    }
    if (domain_num == 0) {
        fclose(stream);
        return;
    }

    qsort(nginx_domain_stats, domain_num, sizeof(nginx_domain_stats[0]), nginxcmp);

    for (i=0; i< top_domain; i++) {
        pos += snprintf(buf + pos, LEN_1M - pos, "%s=%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld" ITEM_SPLIT,
                       nginx_domain_stats[i].domain, nginx_domain_stats[i].nconn, nginx_domain_stats[i].nreq, nginx_domain_stats[i].n2XX, nginx_domain_stats[i].n3XX, nginx_domain_stats[i].n4XX, nginx_domain_stats[i].n5XX, nginx_domain_stats[i].rt, nginx_domain_stats[i].uprt, nginx_domain_stats[i].upreq, nginx_domain_stats[i].upactreq);
        if (strlen(buf) == LEN_1M - 1) {
            fclose(stream);
            close(sockfd);
            return;
        }
    }

    set_mod_record(mod, buf);
    fclose(stream);
    close(sockfd);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--nginx_domain", nginx_domain_usage, nginx_info, 10, read_nginx_domain_stats, set_nginx_domain_record);
}

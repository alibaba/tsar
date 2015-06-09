#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"
#include <stdio.h>

#define MAX 16
int method_num = 0;

struct stats_erpc {
    int port;
    char name [256];           /* method name */
    unsigned long long rt;     /* total bytes in */
    unsigned long long reqc;   /* total bytes out */
    unsigned long long reqs;   /* total connections */
    unsigned long long rspc;   /* total requests */
    unsigned long long rsps;   /* 2XX status code */
};

/* struct for http domain */
static struct stats_erpc erpc_stats[MAX];

static char *erpc_usage = "    --erpc              erpc statistics";

static struct mod_info erpc_info[] = {
    {"    rt", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"reqqps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" reqsz", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {"rspqps", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
    {" rspsz", SUMMARY_BIT, MERGE_SUM,  STATS_NULL},
};

static void erpc_init(char *parameter)
{
    FILE    *fp;
    int      port;
    char    *p, *pp, *line = NULL;
    size_t   size = LEN_1024;
    ssize_t  ret = 0;

    method_num = 0;
    memset(erpc_stats, 0, MAX * sizeof(struct stats_erpc));

    fp = fopen(parameter, "r");
    if (fp == NULL) {
        return;
    }

    while ((ret = getline(&line, &size, fp)) > 0) {
        *(line + strlen(line) - 1) = '\0';
        if ((p = strchr(line, '=')) == NULL) {
            continue;
        }
        port = atoi(line);
        *p++ = '\0';
        pp = p;
        while ((p = strchr(p, ',')) != NULL) {
            *p++ = '\0';
            if (method_num >= MAX) {
                method_num = MAX;
                break;
            }
            erpc_stats[method_num].port = port;
            strcpy(erpc_stats[method_num].name, pp);
            method_num++;
            pp = p;
        }
        if (method_num < MAX) {
            erpc_stats[method_num].port = port;
            strcpy(erpc_stats[method_num].name, pp);
            method_num++;
        }
    }

    if (line != NULL) {
        free(line);
    }
}

static void
set_erpc_record(struct module *mod, double st_array[],
        U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    if (cur_array[0] >= pre_array[0] && cur_array[3] > pre_array[3]) {
        st_array[0] = (cur_array[0] - pre_array[0]) * 1.0 / (cur_array[3] - pre_array[3]) / 1000;
    } else {
        st_array[0] = 0;
    }
    for (i = 1; i < 5; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
        } else {
            st_array[i] = 0;
        }
    }
}

void
read_erpc_stats(struct module *mod, char *parameter)
{
    int                 i, addr_len,  m, sockfd, send, pos = 0, mark = 0;
    char                buf[LEN_1M], request[LEN_4096], line[LEN_4096];
    void               *addr;
    FILE               *stream = NULL;
    struct sockaddr_in  servaddr;
    //struct stats_erpc  stat;

    erpc_init(parameter);

    addr = &servaddr;
    addr_len = sizeof(servaddr);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

    for (i = 0; i < method_num; i++) {
        servaddr.sin_port = htons(erpc_stats[i].port);
        /* send request */
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            return;
        }

        sprintf(request,
                "GET %s HTTP/1.0\r\n"
                "Host: %s\r\n"
                "Accept:*/*\r\n"
                "Connection: Close\r\n\r\n",
                "/monitor", "127.0.0.1");

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
        mark = 0;
        while (fgets(line, LEN_4096, stream) != NULL) {
            if (strstr(line, "methodName") != NULL) {
                if (strstr(line, erpc_stats[i].name)) {
                    mark = 1;
                } else {
                    mark = 0;
                }
            }
            if (mark == 1) {
                if(!strncmp(line, "processTime(us)", 15)) {
                    sscanf(line + 16, "%llu", &erpc_stats[i].rt);
                } else if (!strncmp(line, "requestCount", 12)) {
                    sscanf(line + 13, "%llu", &erpc_stats[i].reqc);
                } else if (!strncmp(line, "requestSize(bytes)", 18)) {
                    sscanf(line + 19, "%llu", &erpc_stats[i].reqs);
                } else if (!strncmp(line, "responseCount", 13)) {
                    sscanf(line + 14, "%llu", &erpc_stats[i].rspc);
                } else if (!strncmp(line, "responseSize(bytes)", 19)) {
                    sscanf(line + 20, "%llu", &erpc_stats[i].rsps);
                }
            }
        }
        fclose(stream);
        close(sockfd);
    }

    for (i = 0; i < method_num; i++) {
        pos += snprintf(buf + pos, LEN_1M - pos, "%d_%s=%lld,%lld,%lld,%lld,%lld" ITEM_SPLIT, erpc_stats[i].port, erpc_stats[i].name, erpc_stats[i].rt, erpc_stats[i].reqc, erpc_stats[i].reqs, erpc_stats[i].rspc, erpc_stats[i].rsps);

        if (strlen(buf) == LEN_1M - 1) {
            return;
        }
    }

    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--erpc", erpc_usage, erpc_info, 5, read_erpc_stats, set_erpc_record);
}

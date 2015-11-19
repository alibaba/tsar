#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"
#include <stdio.h>

struct stats_nginx_live {
    unsigned long long online;      /* 0 */
    unsigned long long olhstr;      /* 1 */
    unsigned long long olvary;      /* 2 */
    unsigned long long upflow;      /* 3 */
    unsigned long long uspeed;      /* 4 */
    unsigned long long downfl;      /* 5 */
    unsigned long long dspeed;      /* 6 */
    unsigned long long fmtime;      /* 7 */
    unsigned long long fmdata;      /* 8 */
    unsigned long long dropfr;      /* 9 */
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_live_usage = "    --nginx_live        nginx live statistics";

static struct mod_info nginx_info[] = {
    {"online", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*0 sum(online of all uri)*/
    {"olhstr", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*1 sum(online_history of all uri)*/
    {"olvary", HIDE_BIT   , MERGE_NULL,  STATS_NULL},  /*2 now(olhstr) - old(olhstr)*/
    {"upflow", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*3 sum(up_flow)*/
    {"uspeed", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*4 diff(upflow) / interval */
    {"downfl", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*5 sum(down_flow) */
    {"dspeed", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*6 diff(downfl) / interval)*/
    {"fmtime", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},  /*7 diff(fmdata) / diff(olhstr)*/
    {"fmdata", HIDE_BIT   , MERGE_NULL,  STATS_NULL},  /*8 sum(fm_time) */
    {"dropfr", SUMMARY_BIT, MERGE_NULL,  STATS_NULL}   /*9 sum(drop_frame)*/
};

static void
set_nginx_live_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;

    for (i = 0; i < 2; i++) {
        st_array[i] = cur_array[i];
    }

    if (cur_array[2] >= pre_array[2]) {
        st_array[2] = cur_array[2] - pre_array[2];
    }

    st_array[3] = cur_array[3];
    st_array[5] = cur_array[5];
    st_array[9] = cur_array[9];

    /*up flow speed*/
    if (cur_array[3] >= pre_array[3]) {
        st_array[4] = (cur_array[3] - pre_array[3]) * 1.0 / inter;
    }

    /*down flow speed*/
    if (cur_array[5] >= pre_array[5]) {
        st_array[6] = (cur_array[5] - pre_array[5]) * 1.0 / inter;
    }

    /*fmtime = diff(sum(fm_time)) / diff(sum(online_history))*/
    if (cur_array[7] >= pre_array[7] && cur_array[1] > pre_array[1]) {
        st_array[7] = (cur_array[8] - pre_array[8]) * 1.0 / (cur_array[1] - pre_array[1]);
    }
}

static void
init_nginx_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("NGX_TSAR_LIVE_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("NGX_TSAR_LIVE_PORT");
    p->port = port ? atoi(port) : 7001;

    p->uri = getenv("NGX_TSAR_LIVE_URI");
    p->uri = p->uri ? p->uri : "/rtmp_reqstat";

    p->server_name = getenv("NGX_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "status.taobao.com";
}

void
read_nginx_live_stats(struct module *mod, char *parameter)
{
    int                 i,addr_len, domain, m, sockfd, send, pos = 0;
    char                buf[LEN_1M], request[LEN_4096], line[LEN_4096];
    unsigned long long online = 0, online_history = 0, up_flow = 0;
    unsigned long long down_flow = 0, fmtime = 0, drop_frame = 0;
    char               *p;
    void               *addr;
    FILE               *stream = NULL;
    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;
    struct stats_nginx_live  stat;

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

    memset(&stat, 0, sizeof(struct stats_nginx_live));
    while (fgets(line, LEN_4096, stream) != NULL) {
        if ((p = strstr(line, "fm_time")) == NULL) {
            continue;
        }

        if (sscanf(p, "fm_time:%llu drop_frame:%llu online:%llu online_history:%llu down_flow:%llu up_flow:%llu",
                   &fmtime, &drop_frame, &online, &online_history, &down_flow, &up_flow) != 6) {
            continue;
        }
        stat.online += online;
        stat.fmdata += fmtime;
        stat.dropfr += drop_frame;
        stat.olhstr += online_history;
        stat.downfl += down_flow;
        stat.upflow += up_flow;
    }
    pos += snprintf(buf + pos, LEN_1M - pos, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld", stat.online, stat.olhstr, stat.olvary, stat.upflow, stat.uspeed, stat.downfl, stat.dspeed, stat.fmtime, stat.fmdata, stat.dropfr);
    if (strlen(buf) >= LEN_1M - 1) {
        fclose(stream);
        close(sockfd);
        return;
    }

    set_mod_record(mod, buf);
    fclose(stream);
    close(sockfd);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--nginx_live", nginx_live_usage, nginx_info, 10, read_nginx_live_stats, set_nginx_live_record);
}

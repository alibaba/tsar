#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"

struct stats_tmd {
    unsigned long long nprocess;    /* tmd process requests */
    unsigned long long ncheckcode;  /* return checkcode requests */
    unsigned long long nwait;       /* return wait requests */
    unsigned long long ndeny;       /* return deny requests */
};

struct hostinfo {
    int    port;
    char  *host;
    char  *server_name;
    char  *uri;
};

static char *tmd_usage = "    --tmd               tmd statistics";

static struct mod_info tmd_info[] = {
    {"proces", DETAIL_BIT,  0,  STATS_NULL},
    {" check", DETAIL_BIT,  0,  STATS_NULL},
    {"  wait", DETAIL_BIT,  0,  STATS_NULL},
    {"  deny", DETAIL_BIT,  0,  STATS_NULL}
};


static void 
set_tmd_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 4; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;

        } else {
            st_array[i] = -1;
        }
    }
}


static void
init_tmd_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("TMD_TSAR_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("TMD_TSAR_PORT");
    p->port = port ? atoi(port) : 80;

    p->uri = getenv("TMD_TSAR_URI");
    p->uri = p->uri ? p->uri : "/tmd_status";

    p->server_name = getenv("TMD_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "tmd.status.taobao.com";
}


void
read_tmd_stats(struct module *mod)
{
    int    write_flag = 0, addr_len, domain;
    int    m, sockfd, send, pos;
    void  *addr;
    char   buf[LEN_4096], request[LEN_4096], line[LEN_4096];

    struct sockaddr_in servaddr;
    struct sockaddr_un servaddr_un;
    FILE  *stream = NULL;
    struct hostinfo hinfo;
    init_tmd_host_info(&hinfo);
    struct stats_tmd st_tmd;
    memset(&st_tmd, 0, sizeof(struct stats_tmd));

    if (*hinfo.host == '/') {
        addr = &servaddr_un;
        addr_len = sizeof(servaddr_un);
        bzero(addr, addr_len);
        domain = AF_LOCAL;
        servaddr_un.sun_family = AF_LOCAL;
        strncpy(servaddr_un.sun_path, hinfo.host,
                sizeof(servaddr_un.sun_path) - 1);

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

        if (!strncmp(line, " ", 1)) {

            sscanf(line + 1, "%llu %llu %llu %llu",
                    &st_tmd.nprocess, &st_tmd.ncheckcode, &st_tmd.nwait,
                    &st_tmd.ndeny);

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
        pos = sprintf(buf, "%lld,%lld,%lld,%lld", st_tmd.nprocess,
                st_tmd.ncheckcode, st_tmd.nwait, st_tmd.ndeny);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--tmd", tmd_usage, tmd_info, 4,
            read_tmd_stats, set_tmd_record);
}

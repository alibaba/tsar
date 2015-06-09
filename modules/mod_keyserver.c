#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"

struct stats_keyserver {
    unsigned long long nrequests;   /* key server requests */
    unsigned long long nfailed;     /* failed requests */
    unsigned long long nrt;         /* average dec/enc time */
};

struct hostinfo {
    int    port;
    char  *host;
    char  *server_name;
    char  *uri;
};

static char *keyserver_usage = "    --keyserver              key server statistics";

static struct mod_info keyserver_info[] = {
    {"  reqs", DETAIL_BIT,  0,  STATS_NULL},
    {"failed", DETAIL_BIT,  0,  STATS_NULL},
    {"    rt", SUMMARY_BIT,  0,  STATS_NULL},
};


static void
set_keyserver_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 3; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;

        } else {
            st_array[i] = -1;
        }
    }
}


static void
init_keyserver_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("KEYSERVER_TSAR_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("KEYSERVER_TSAR_PORT");
    p->port = port ? atoi(port) : 80;

    p->uri = getenv("KEYSERVER_TSAR_URI");
    p->uri = p->uri ? p->uri : "/keyserver_status";

    p->server_name = getenv("KEYSERVER_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "keyserver.status.taobao.com";
}


void
read_keyserver_stats(struct module *mod, char *parameter)
{
    int    write_flag = 0, addr_len, domain;
    int    m, sockfd, send, pos;
    void  *addr;
    char   buf[LEN_4096], request[LEN_4096], line[LEN_4096];

    struct sockaddr_in servaddr;
    struct sockaddr_un servaddr_un;
    FILE  *stream = NULL;
    struct hostinfo hinfo;
    init_keyserver_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_keyserver st_ks;
    memset(&st_ks, 0, sizeof(struct stats_keyserver));

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

            sscanf(line + 1, "%llu %llu %llu",
                    &st_ks.nrequests, &st_ks.nfailed, &st_ks.nrt);

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
        pos = sprintf(buf, "%lld,%lld,%lld",
                    st_ks.nrequests, st_ks.nfailed, st_ks.nrt);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--keyserver", keyserver_usage, keyserver_info, 3,
            read_keyserver_stats, set_keyserver_record);
}

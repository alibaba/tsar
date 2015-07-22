#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"


#define sub(a, b) ((a) <= (b) ? 0 : (a) - (b))

struct stats_pharos_qtype {
    unsigned long long a;
    unsigned long long aaaa;
    unsigned long long cname;
    unsigned long long srv;
    unsigned long long ns;
    unsigned long long mx;
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *pharos_qtype_usage = "    --pharos-qtype            Pharos qtype statistics";

static struct mod_info pharos_qtype_info[] = {
    {"     a",  DETAIL_BIT,  0, STATS_NULL},
    {"  aaaa",  DETAIL_BIT,  0, STATS_NULL},
    {" cname",  DETAIL_BIT,  0, STATS_NULL},
    {"   srv",  DETAIL_BIT,  0, STATS_NULL},
    {"    ns",  DETAIL_BIT,  0, STATS_NULL},
    {"    mx",  DETAIL_BIT,  0, STATS_NULL},
};


static void
set_pharos_qtype_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = sub(cur_array[0], pre_array[0]); // a
    st_array[1] = sub(cur_array[1], pre_array[1]); // aaaa
    st_array[2] = sub(cur_array[2], pre_array[2]); // cname
    st_array[3] = sub(cur_array[3], pre_array[3]); // srv
    st_array[4] = sub(cur_array[4], pre_array[4]); // ns
    st_array[5] = sub(cur_array[5], pre_array[5]); // mx
}

static void
init_pharos_qtype_host_info(struct hostinfo *p)
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
read_pharos_qtype_stats(struct module *mod, char *parameter)
{
    int                 write_flag = 0, addr_len, domain;
    int                 m, sockfd, send, pos;
    void               *addr;
    char                buf[LEN_4096], request[LEN_4096], line[LEN_4096];
    FILE               *stream = NULL;

    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;

    init_pharos_qtype_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_pharos_qtype st_pharos_qtype;
    memset(&st_pharos_qtype, 0, sizeof(struct stats_pharos_qtype));

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
        // read retcode
        if (!strncmp(line, "qtype:", sizeof("qtype:") - 1)) {
            sscanf(line, "qtype:A=%lld,AAAA=%lld,CNAME=%lld,SRV=%lld,NS=%lld,MX=%lld",
                    &st_pharos_qtype.a,
                    &st_pharos_qtype.aaaa,
                    &st_pharos_qtype.cname,
                    &st_pharos_qtype.srv,
                    &st_pharos_qtype.ns,
                    &st_pharos_qtype.mx);

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
        pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld",
                      st_pharos_qtype.a,
                      st_pharos_qtype.aaaa,
                      st_pharos_qtype.cname,
                      st_pharos_qtype.srv,
                      st_pharos_qtype.ns,
                      st_pharos_qtype.mx);

        buf[pos] = '\0';
        set_mod_record(mod, buf);
    }
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pharos-qtype", pharos_qtype_usage, pharos_qtype_info, 6,
                        read_pharos_qtype_stats, set_pharos_qtype_record);
}

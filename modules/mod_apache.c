#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "tsar.h"

char *apache_usage = "    --apache            apache statistics";

struct stats_apache {
    unsigned int        busy_proc;
    unsigned int        idle_proc;
    unsigned long long  query;
    unsigned long long  response_time;
    unsigned long long  kBytes_sent;
};

#define STATS_APACHE_SIZE (sizeof(struct stats_apache))


struct hostinfo {
    char *host;
    int port;
};

void
init_host_info(struct hostinfo *p)
{
    p->host = strdup("127.0.0.1");
    p->port = 80;
}

void
read_apache_stats(struct module *mod)
{
    int fd, n, m, sockfd, send, pos;
    char buf[LEN_4096] = {0}, request[LEN_4096], line[LEN_4096],
         buff[LEN_4096];
    memset(buf, 0, LEN_4096);
    struct sockaddr_in servaddr;
    FILE *stream = NULL;
    /* FIX me */
    char *cmd = "server-status?auto";
    struct stats_apache st_apache;
    memset(&st_apache, 0, sizeof(struct stats_apache));
    struct hostinfo hinfo;
    memset(&hinfo, 0, sizeof(struct hostinfo));

    if ((fd = open(APACHERT, O_RDONLY , 0644)) < 0 ){
        return;
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        goto last;
    }
    if ((n = read(fd, buff, 16)) != 16) {
        goto last;
    }
    st_apache.query = * (unsigned long long *)buff;
    st_apache.response_time = * (unsigned long long *)&buff[8];
    /*fullfil another member int the structure*/
    init_host_info(&hinfo);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(hinfo.port);
    inet_pton(AF_INET, hinfo.host, &servaddr.sin_addr);
    sprintf(request,
            "GET /%s HTTP/1.0\r\n"
            "User-Agent: Wget/1.9\r\n"
            "Host: %s\r\n"
            "Accept:*/*\r\n"
            "Connection: Close\r\n\r\n",
            cmd, hinfo.host);

    if ((m = connect(sockfd, (struct sockaddr *) &servaddr,
                    sizeof(servaddr))) == -1 ) {
        goto writebuf;
    }

    if ((send = write(sockfd, request, strlen(request))) == -1) {
        goto writebuf;
    }
    stream = fdopen(sockfd, "r");
    if (!stream) {
        goto last;
    }
    while(fgets(line, LEN_4096, stream) != NULL) {
        if(!strncmp(line, "Total kBytes:", 13)) {
            sscanf(line + 14, "%llu", &st_apache.kBytes_sent);

        } else if (!strncmp(line, "BusyWorkers:", 12)) {
            sscanf(line + 13, "%d", &st_apache.busy_proc);

        } else if (!strncmp(line, "IdleWorkers:", 12)) {
            sscanf(line + 13, "%d", &st_apache.idle_proc);

        } else {
            ;
        }
        memset(line, 0, LEN_4096);
    }
writebuf:
    pos = sprintf(buf, "%lld,%lld,%lld,%d,%d",
            st_apache.query,
            st_apache.response_time / 1000,
            st_apache.kBytes_sent,
            st_apache.busy_proc,
            st_apache.idle_proc);
    buf[pos] = '\0';
    if (stream) {
        if (fclose(stream) < 0) {
            goto last;
        }
    }
    set_mod_record(mod, buf);
last:
    close(fd);
    if (sockfd) {
        close(sockfd);
    }
    free(hinfo.host);
}

static void
set_apache_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    /* set st record */
    if(cur_array[0] >= pre_array[0])
        st_array[0] = (cur_array[0] - pre_array[0]) / (inter * 1.0);
    if((cur_array[1] >= pre_array[1]) && (cur_array[0] > pre_array[0]))
        st_array[1] = (cur_array[1] - pre_array[1]) / ((cur_array[0] - pre_array[0]) * 1.0);
    if(cur_array[2] >= pre_array[2])
        st_array[2] = (cur_array[2] - pre_array[2]) / (inter * 1.0);
    for (i = 3; i <= 4; i++) st_array[i] = cur_array[i];
}

static struct mod_info apache_info[] = {
    {"   qps", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {"    rt", SUMMARY_BIT,  0,  STATS_SUB_INTER},
    {"  sent", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"  busy", DETAIL_BIT,  0,  STATS_NULL},
    {"  idle", DETAIL_BIT,  0,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--apache", apache_usage, apache_info, 5, read_apache_stats, set_apache_record);
}

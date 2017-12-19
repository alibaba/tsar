#define   _GNU_SOURCE
#include "tsar.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/var/run/haproxy.stat"
#define HAPROXY "/var/run/haproxy.pid"
#define HAPROXY_STORE_FMT(d)            "%ld"d"%ld"d"%ld"d"%ld"d"%ld"d"%ld"
#define server_port 80
#define server_address "127.0.0.1"
#define URL "http://img01.taobaocdn.com/health.gif"
#define CRLF "\r\n"
#define DEBUG 0
#define RETRY 3
#define MAX_HOST_ADDRESS_LENGTH 64
#define MAX_INPUT_BUFFER 1024
#define MAX_SIZE 40960
#define my_recv(buf, len) read(sd, buf, len)
#define my_send(buf, len) send(sd, buf, len, 0)
#define my_tcp_connect(addr, port, s) np_net_connect(addr, port, s, "tcp")
int np_net_connect(const char *address, int port, int *sd, char* proto);
int get_http_status();
int StrToInt(const char* str);
int get_haproxy_detail();
int sd;
int was_refused = 0;
int econn_refuse_state = 2;
int address_family = AF_INET;
char buffer[MAX_INPUT_BUFFER];

struct stats_haproxy {
    unsigned long stat;
    unsigned long uptime;
    unsigned long conns;
    unsigned long qps;
    unsigned long hit;
    unsigned long rt;
};

static struct mod_info info[] = {
    {"  stat", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},
    {"uptime", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},
    {" conns", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {"   qps", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},
    {"   hit", SUMMARY_BIT,  MERGE_NULL,  STATS_NULL},
    {"    rt", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
};

static char *haproxy_usage = "    --haproxy           haproxy usage";
struct stats_haproxy st_haproxy;

/*
 *******************************************************
 * Read swapping statistics from haproxy.stat & 80 port
 *******************************************************
 */
static void
read_haproxy(struct module *mod)
{
    int i, pos=0;
    char buf[512];

    memset(&st_haproxy, 0, sizeof(struct stats_haproxy));
    for (i = 0;i < RETRY;i++) {
        if (get_http_status() == 0 && access(HAPROXY, 0) == 0) {
            st_haproxy.stat = 1;
            break;
        }
    }
    if (st_haproxy.stat == 1 && get_haproxy_detail() == 0) {
        if (DEBUG) {
            printf("get right.\n");
        }

    } else {
        if (DEBUG) {
            printf("get wrong.\n");
        }
    }
    if (st_haproxy.stat == 1) {
        pos = sprintf(buf, HAPROXY_STORE_FMT(DATA_SPLIT), st_haproxy.stat, st_haproxy.uptime,
                      st_haproxy.conns, st_haproxy.qps, st_haproxy.hit, st_haproxy.rt);
    }
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    return;
}


static void
set_haproxy_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i <3; i++) {
        st_array[i] = cur_array[i];
    }
    if (cur_array[3] > pre_array[3]) {
        st_array[3] = (cur_array[3] - pre_array[3]) / inter;
    } else {
        st_array[3] = 0;
    }
    st_array[4] = cur_array[4] / 10.0;
    st_array[5] = cur_array[5] / 100.0;
}


void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--haproxy", haproxy_usage, info, sizeof(info) / sizeof(struct mod_info), read_haproxy, set_haproxy_record);
}


/* Returns 1 if we're done processing the document body; 0 to keep going */
static int
document_headers_done (char *full_page)
{
    const char *body;

    for (body = full_page; *body; body++) {
        if (!strncmp (body, "\n\n", 2) || !strncmp (body, "\n\r\n", 3))
            break;
    }

    if (!*body)
        return 0;  /* haven't read end of headers yet */

    full_page[body - full_page] = 0;
    return 1;
}

int
get_http_status()
{
    if (my_tcp_connect (server_address, server_port, &sd) == 0) {
        close(sd);
        if (DEBUG) {
            printf("connect host %s at %d\n", server_address, server_port);
        }
        return 0;
    } else {
        close(sd);
        return -1;
        if (DEBUG) {
            printf("cat't connect host %s at %d\n", server_address, server_port);
        }
    }
    int     i = 0;
    int     http_status;
    char   *buf;
    char   *full_page;
    char   *page;
    char   *status_line;
    char   *status_code;
    size_t  pagesize = 0;

    if (asprintf (&buf, "GET %s HTTP/1.0\r\nUser-Agent: check_http\r\n", URL) < 0) {
        return -1;
    }
    /* tell HTTP/1.1 servers not to keep the connection alive */
    if (asprintf (&buf, "%sConnection: close\r\n", buf) < 0) {
        return -1;
    }
    if (server_address) {
        if (asprintf (&buf, "%sHost: %s:%d\r\n", buf, server_address, server_port) < 0) {
            return -1;
        }
    }
    if (asprintf (&buf, "%s%s", buf, CRLF) < 0){
        return -1;
    }
    if (DEBUG) {
        printf ("send %s\n", buf);
    }
    my_send (buf, strlen (buf));
    full_page = strdup("");
    while ((i = my_recv (buffer, MAX_INPUT_BUFFER-1)) > 0) {
        buffer[i] = '\0';
        if (asprintf (&full_page, "%s%s", full_page, buffer) < 0) {
            return -1;
        }
        pagesize += i;
        if (document_headers_done (full_page)) {
            i = 0;
            break;
        }
    }
    if (DEBUG) {
        printf("%s\n", full_page);
    }
    if (i < 0 && errno != ECONNRESET) {
        printf("HTTP CRITICAL - Error on receive\n");
    }
    /* return a CRITICAL status if we couldn't read any data */
    if (pagesize == (size_t) 0){
        printf("HTTP CRITICAL - No data received from host\n");
    }
    /* close the connect */
    if (sd) {
        close(sd);
    }


    /* find status line and null-terminate it */
    page = full_page;
    page += (size_t) strspn (page, "\r\n");
    status_line = page;
    status_line[strcspn(status_line, "\r\n")] = 0;
    if (DEBUG) {
        printf ("STATUS: %s\n", status_line);
    }
    status_code = strchr (status_line, ' ') + sizeof (char);
    http_status = atoi (status_code);
    return http_status;
}


/* opens a tcp or udp connection to a remote host or local socket */
int
np_net_connect (const char *host_name, int port, int *sd, char* proto)
{
    int                 result;
    struct protoent    *ptrp;
    struct sockaddr_in  servaddr;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, server_address, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp = getprotobyname(proto))) == NULL) {
        if (DEBUG) {
            printf("Cannot map \"%s\" to protocol number\n", proto);
        }
        return 3;
    }

    /* create a socket */
    *sd = socket(PF_INET, (!strcmp(proto, "udp"))?SOCK_DGRAM:SOCK_STREAM, ptrp->p_proto);
    if (*sd < 0) {
        close(*sd);
        if (DEBUG) {
            printf("Socket creation failed\n");
        }
        return 3;
    }
    /* open a connection */
    result = connect(*sd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (result < 0) {
        close(*sd);
        switch(errno) {
            case ECONNREFUSED:
                if (DEBUG) {
                    printf("Connection refused by host\n");
                }
                break;
            case ETIMEDOUT:
                if (DEBUG) {
                    printf("Timeout while attempting connection\n");
                }
                break;
            case ENETUNREACH:
                if (DEBUG) {
                    printf("Network is unreachable\n");
                }
                break;
            default:
                if (DEBUG) {
                    printf("Connection refused or timed out\n");
                }
        }

        return 2;
    }
    return 0;
}

int
StrToInt(const char* str)
{
    int num = 0;
    if (str != NULL) {
        const char* digit = str;
        while (*digit != '\0') {
            if (*digit >= '0' && *digit <= '9') {
                num = num * 10 + (*digit - '0');
            } else {
                num = 0;
                break;
            }
            digit ++;
        }
    }
    return num;
}

int
get_haproxy_detail(void)
{
    int                 s, t, len;
    char                str[MAX_SIZE];
    char                show_info[20] = "show info\n";
    char                show_tsar[20] = "show tsar\n";
    char               *p_split;
    struct sockaddr_un  remote;
    /*result for tsar to show*/
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        if (DEBUG) {
            perror("socket");
        }
        return -1;
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        close(s);
        if (DEBUG) {
            perror("connect");
        }
        return -1;
    }
    memset(str, '\0', sizeof(str));
    if (send(s, show_tsar, strlen(show_tsar), 0) == -1) {
        close(s);
        if (DEBUG) {
            perror("send");
        }
        return -1;
    }

    t = recv(s, str, MAX_SIZE, 0);
    close(s);
	if (t > 0) {
        str[t] = '\0';
    } else {
        if (t < 0 && DEBUG) {
            perror("recv");
        } else if (DEBUG) {
            perror("Server closed connection\n");
        }
        return -1;
    }

    if (!strstr(str, "Uptime_sec")) {
        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            if (DEBUG) {
                perror("socket");
            }
            return -1;
        }
        if (connect(s, (struct sockaddr *)&remote, len) == -1) {
            close(s);
            if (DEBUG) {
                perror("connect");
            }
            return -1;
        }
        memset(str, '\0', sizeof(str));
        if (send(s, show_info, strlen(show_info), 0) == -1) {
            close(s);
            if (DEBUG) {
                perror("send");
            }
            return -1;
        }

        if ((t=recv(s, str, MAX_SIZE, 0)) > 0) {
            str[t] = '\0';
        } else {
            close(s);
            if (t < 0 && DEBUG) {
                perror("recv");
            } else if (DEBUG) {
                perror("Server closed connection\n");
            }
            return -1;
        }
        close(s);
    }
    p_split = strtok(str, "\n");
    while (p_split) {
        if (strstr(p_split, "total request num:")) {
            sscanf(p_split, "total request num: %ld/", &st_haproxy.qps);
        }
        if (strstr(p_split, "total request num/total hit request num/ total conns num:")) {
            sscanf(p_split, "total request num/total hit request num/ total conns num: %ld/", &st_haproxy.qps);
        }
        if (strstr(p_split, "mean rt:")) {
            int a, b;
            sscanf(p_split, "mean rt: %d.%d (ms)", &a, &b);
            st_haproxy.rt = a * 100 + b;
        }
        if (strstr(p_split, "req hit ratio:")) {
            int a, b;
            sscanf(p_split, "req hit ratio: %d.%d ", &a, &b);
            st_haproxy.hit = a * 1000 + b;
        }
        if (strstr(p_split, "Uptime_sec")) {
            char *p = strstr(p_split, " ");
            st_haproxy.uptime = StrToInt(p + 1);
        }
        if (strstr(p_split, "CurrConns")) {
            char *p = strstr(p_split, " ");
            st_haproxy.conns = StrToInt(p + 1);
        }
        p_split = strtok(NULL, "\n");
    }
    return 0;
}

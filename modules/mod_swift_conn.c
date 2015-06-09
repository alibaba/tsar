#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"

#define RETRY_NUM 4
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 82
#define EQUAL ":="
#define DEBUG 1

char *swift_conn_usage = "    --swift_conn         Swift connection infomation";
int mgrport = 82;

const static char *SWIFT_STORE[] = {
    "client_http.accepts",
    "client_http.conns",
    "server_http.on_connects",
    "server_http.conns",
    "server_http.wait_conns"
};

/* struct for swift counters */
struct status_swift_conn {
    unsigned long long c_act;
    unsigned long long c_conn;
    unsigned long long s_act;
    unsigned long long s_conn;
    unsigned long long s_wait;
} stats;

/* swift register info for tsar */
struct mod_info swift_conn_info[] = {
    {" c_act", DETAIL_BIT, 0, STATS_NULL},
    {"c_conn", DETAIL_BIT, 0, STATS_NULL},
    {" s_act", DETAIL_BIT, 0, STATS_NULL},
    {"s_conn", DETAIL_BIT, 0, STATS_NULL},
    {"s_wait", DETAIL_BIT, 0, STATS_NULL},
    {"  null", HIDE_BIT, 0, STATS_NULL}
};

/* opens a tcp or udp connection to a remote host or local socket */
static int
my_swift_net_connect(const char *host_name, int port, int *sd, char* proto)
{
    int			result;
    struct protoent	*ptrp;
    struct sockaddr_in	servaddr;

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET, host_name, &servaddr.sin_addr);

    /* map transport protocol name to protocol number */
    if (((ptrp=getprotobyname(proto)))==NULL) {
        if (DEBUG) {
	    printf("Cannot map \"%s\" to protocol number\n", proto);
	}
	return 3;
    }

    /* create a connection */
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
	switch (errno) {
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

static ssize_t
mywrite_swift(int fd, void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

static ssize_t
myread_swift(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}

/* get value from counter */
static int
read_swift_value(char *buf, const char *key, unsigned long long *ret)
{
    int    k = 0;
    char  *tmp;
    /* is str match the keywords? */
    if ((tmp = strstr(buf, key)) != NULL) {
        /* compute the offset */
        k = strcspn(tmp, EQUAL);
        sscanf(tmp + k + 1, "%lld", ret);
        return 1;

    } else {
        return 0;
    }
}

static int
parse_swift_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_value(line, SWIFT_STORE[0], &stats.c_act);
        read_swift_value(line, SWIFT_STORE[1], &stats.c_conn);
        read_swift_value(line, SWIFT_STORE[2], &stats.s_act);
        read_swift_value(line, SWIFT_STORE[3], &stats.s_conn);
        read_swift_value(line, SWIFT_STORE[4], &stats.s_wait);
        line = strtok(NULL, "\n");
    }
    return 0;
}

static void
set_swift_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    /* client accepts */
    if (cur_array[0] > pre_array[0])
        st_array[0] = (cur_array[0] - pre_array[0]) / inter;
    else
        st_array[0] = 0;

    /* client conns */
    if (cur_array[1] > 0)
        st_array[1] = cur_array[1];
    else
        st_array[1] = 0;

    /* server accepts */
    if (cur_array[2] > pre_array[2])
        st_array[2] = (cur_array[2] - pre_array[2]) / inter;
    else
        st_array[2] = 0;

    /* server conn */
    if (cur_array[3] > 0)
        st_array[3] = cur_array[3];
    else
        st_array[3] = 0;

    /* server wait */
    if (cur_array[4] > 0)
        st_array[4] = cur_array[4];
    else
        st_array[4] = 0;
}

static int
read_swift_stat(char *cmd)
{
    char msg[LEN_512];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/%s "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n",
            cmd);

    int len, conn = 0, bytesWritten, fsize = 0;

    if (my_swift_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    int flags;

    /* set socket fd noblock */
    if ((flags = fcntl(conn, F_GETFL, 0)) < 0) {
        close(conn);
        return -1;
    }

    if (fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(conn);
        return -1;
    }

    struct timeval timeout = {10, 0};

    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

    bytesWritten = mywrite_swift(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift(conn, buf, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    /* read error */
    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_swift_info(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

static void
read_swift_stats(struct module *mod, char *parameter)
{
    int    retry = 0, pos = 0;
    char   buf[LEN_4096];

    memset(&stats, 0, sizeof(stats));
    mgrport = atoi(parameter);
    if (!mgrport) {
        mgrport = 82;
    }
    retry = 0;
    while (read_swift_stat("counters") < 0 && retry < RETRY_NUM) {
        retry++;
    }
    pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld",
             stats.c_act,
             stats.c_conn,
             stats.s_act,
             stats.s_conn,
             stats.s_wait
               );

    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--swift_conn", swift_conn_usage, swift_conn_info, 5, read_swift_stats, set_swift_record);
}

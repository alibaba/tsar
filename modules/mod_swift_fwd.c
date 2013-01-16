#include <sys/socket.h>
#include <fcntl.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 81
#define EQUAL "="
#define DEBUG 0

char *swift_fwd_usage = "    --swift_fwd             Swift source infomation";

/* swiftclient -p 81 mgr:counters */
/*
server_http.requests = 13342113
server_http.errors = 220
server_http.bytes_in = 210982517709
server_http.bytes_out = 0
server_http.svc_time = 1526450363
*/
const static char *SWIFT_FWD[] = {
	"server_http.requests",
	"server_http.errors",
	"server_http.bytes_in",
	"server_http.svc_time"
};

/* struct for httpfwd counters */
struct status_swift_fwd {
	unsigned long long requests;
	unsigned long long errors;
	unsigned long long bytes_in;
	unsigned long long svc_time;
} stats;

/* swift register info for tsar */
struct mod_info swift_fwd_info[] = {
	{"   qps", DETAIL_BIT,  0,  STATS_NULL},
	{" traff", DETAIL_BIT,  0,  STATS_NULL},
	{" error", DETAIL_BIT,  0,  STATS_NULL},
	{"    rt", DETAIL_BIT,  0,  STATS_NULL}
};
/* opens a tcp or udp connection to a remote host or local socket */
int my_swift_fwd_net_connect(const char *host_name, int port, int *sd, char* proto)
{
	struct sockaddr_in servaddr;
	struct protoent *ptrp;
	int result;

	bzero((char *)&servaddr,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(port);
	inet_pton(AF_INET, host_name, &servaddr.sin_addr);

	/* map transport protocol name to protocol number */
	if(((ptrp=getprotobyname(proto)))==NULL){
		if(DEBUG)
			printf("Cannot map \"%s\" to protocol number\n",proto);
		return 3;
	}

	/* create a socket */
	*sd=socket(PF_INET,(!strcmp(proto,"udp"))?SOCK_DGRAM:SOCK_STREAM,ptrp->p_proto);
	if(*sd<0){
		close(*sd);
		if(DEBUG)
			printf("Socket creation failed\n");
		return 3;
	}
	/* open a connection */
	result=connect(*sd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	if(result<0){
		close(*sd);
		switch(errno){
			case ECONNREFUSED:
				if(DEBUG)
					printf("Connection refused by host\n");
				break;
			case ETIMEDOUT:
				if(DEBUG)
					printf("Timeout while attempting connection\n");
				break;
			case ENETUNREACH:
				if(DEBUG)
					printf("Network is unreachable\n");
				break;
			default:
				if(DEBUG)
					printf("Connection refused or timed out\n");
		}

		return 2;
	}
	return 0;
}
ssize_t mywrite_swift_fwd(int fd, void *buf, size_t len)
{
	return send(fd, buf, len, 0);
}

ssize_t myread_swift_fwd(int fd, void *buf, size_t len)
{
	return recv(fd, buf, len, 0);
}

/* get value from counter */
int read_swift_fwd_value(char *buf,
		const char *key,
		unsigned long long *ret)
{
	int k=0;
	char *tmp;
	/* is str match the keywords? */
	if ((tmp = strstr(buf, key)) != NULL) {
		/* compute the offset */
		k = strcspn(tmp, EQUAL);
		sscanf(tmp + k + 1, "%lld", ret);
		return 1;
	} else return 0;
}
int parse_swift_fwd_info(char *buf)
{
	char *line;
	line = strtok(buf, "\n");
	while(line != NULL){
		read_swift_fwd_value(line, SWIFT_FWD[0], &stats.requests);
		read_swift_fwd_value(line, SWIFT_FWD[1], &stats.errors);
		read_swift_fwd_value(line, SWIFT_FWD[2], &stats.bytes_in);
		read_swift_fwd_value(line, SWIFT_FWD[3], &stats.svc_time);
		line = strtok(NULL, "\n");
	}
	return 0;
}

void set_swift_fwd_record(struct module *mod, double st_array[],
		U_64 pre_array[], U_64 cur_array[], int inter)
{
	int i;
	for (i = 0; i < mod->n_col-1; i++) {
		if (cur_array[i] >= pre_array[i]) {
			st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / inter;
		}else
			st_array[i] = -1;
	}
	if(cur_array[i] >= pre_array[i] && st_array[0] > 0){
		st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / st_array[0] / inter;
	}else
		st_array[i] = -1;
}

int read_swift_fwd_stat()
{
	char msg[LEN_512];
	char buf[LEN_4096];
	sprintf(msg,
			"GET cache_object://localhost/counters "
			"HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Accept:*/*\r\n"
			"Connection: close\r\n\r\n");

	int len, conn, bytesWritten, fsize = 0;

	if(my_swift_fwd_net_connect(HOSTNAME, PORT, &conn, "tcp") != 0){
		close(conn);
		return -1;
	}

	int flags;

	/* set socket fd noblock */
	if((flags = fcntl(conn, F_GETFL, 0)) < 0) {
		close(conn);
		return -1;
	}

	if(fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
		close(conn);
		return -1;
	}

	struct timeval timeout = {10, 0};

	setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));

	setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

	bytesWritten = mywrite_swift_fwd(conn, msg, strlen(msg));
	if (bytesWritten < 0) {
		close(conn);
		return -2;
	} else if (bytesWritten != strlen(msg)) {
		close(conn);
		return -3;
	}

	while ((len = myread_swift_fwd(conn, buf, sizeof(buf))) > 0) {
		fsize += len;
	}

	/* read error */
	if (fsize < 100) {
		close(conn);
		return -1;
	}

	if (parse_swift_fwd_info(buf) < 0) {
		close(conn);
		return -1;
	}

	close(conn);
	return 0;
}

void read_swift_fwd_stats(struct module *mod)
{
	int retry = 0 ,pos = 0;
	char buf[LEN_1024];
	memset(&stats, 0, sizeof(stats));
	while(read_swift_fwd_stat() < 0 && retry < RETRY_NUM){
		retry++;
	}
	pos = sprintf(buf, "%lld,%lld,%lld,%lld",
			stats.requests,
			stats.bytes_in,
			stats.errors,
			stats.svc_time
		     );
	buf[pos] = '\0';
	set_mod_record(mod, buf);
}

void mod_register(struct module *mod)
{
	register_mod_fileds(mod, "--swift_fwd", swift_fwd_usage, swift_fwd_info, 4, read_swift_fwd_stats, set_swift_fwd_record);
}

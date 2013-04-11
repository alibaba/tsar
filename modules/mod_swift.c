#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"


#define RETRY_NUM 3
/* swift default port should not changed */
#define HOSTNAME "localhost"
#define PORT 81
#define EQUAL ":="
#define DEBUG 1

char *swift_usage = "    --swift             Swift object storage infomation";
int mgrport=81;

/* string at swiftclient -p 81 mgr:info */
/*
 * Average HTTP respone time:      5min: 11.70 ms, 60min: 10.06 ms
 * Request Hit Ratios:     5min: 95.8%, 60min: 95.7%
 * Byte Hit Ratios:        5min: 96.6%, 60min: 96.6%
 * UP Time:        247256.904 seconds
 * CPU Time:       23487.042 seconds
 * StoreEntries           : 20776287
 * client_http.requests = 150291472
 * client_http.bytes_in = 6380253436
 * client_http.bytes_out = 5730106537327
 */
const static char *SWIFT_STORE[] = {
	"client_http.requests",
	"client_http.bytes_in",
	"client_http.bytes_out",
	"StoreEntries"
};

/* struct for swift counters */
struct status_swift {
	unsigned long long requests;
	unsigned long long rt;
	unsigned long long r_hit;
	unsigned long long b_hit;
	unsigned long long objs;
	unsigned long long bytes_in;
	unsigned long long bytes_out;
	unsigned long long t_cpu;
	unsigned long long s_cpu;
} stats;

/* swift register info for tsar */
struct mod_info swift_info[] = {
	{"   qps", DETAIL_BIT,  0,  STATS_NULL},
	{"    rt", DETAIL_BIT,  0,  STATS_NULL},
	{" r_hit", DETAIL_BIT,  0,  STATS_NULL},
	{" b_hit", DETAIL_BIT,  0,  STATS_NULL},
	{"  objs", DETAIL_BIT,  0,  STATS_NULL},
	{" in_bw", DETAIL_BIT,  0,  STATS_NULL},
	{"out_bw", DETAIL_BIT,  0,  STATS_NULL},
	{"   cpu", DETAIL_BIT,  0,  STATS_NULL},
	{"  null", HIDE_BIT,  0,  STATS_NULL}
};
/* opens a tcp or udp connection to a remote host or local socket */
int my_swift_net_connect(const char *host_name, int port, int *sd, char* proto)
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
ssize_t mywrite_swift(int fd, void *buf, size_t len)
{
	return send(fd, buf, len, 0);
}

ssize_t myread_swift(int fd, void *buf, size_t len)
{
	return recv(fd, buf, len, 0);
}

/* get value from counter */
int read_swift_value(char *buf,
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
int parse_swift_info(char *buf)
{
	char *line;
	line = strtok(buf, "\n");
	while(line != NULL){
		read_swift_value(line, SWIFT_STORE[0], &stats.requests);
		read_swift_value(line, SWIFT_STORE[1], &stats.bytes_in);
		read_swift_value(line, SWIFT_STORE[2], &stats.bytes_out);
		read_swift_value(line, SWIFT_STORE[3], &stats.objs);
		/*Average HTTP respone time:      5min: 11.70 ms, 60min: 10.06 ms*/
		if(strstr(line,"Average HTTP respone time") != NULL){
			float a,b;
			sscanf(line,"        Average HTTP respone time:      5min: %f ms, 60min: %f ms",&a,&b);
			stats.rt = a * 1000;
		}
		/* Request Hit Ratios:     5min: 95.8%, 60min: 95.7% */
		if(strstr(line,"Request Hit Ratios") != NULL){
			float a,b;
			sscanf(line,"        Request Hit Ratios:     5min: %f%%, 60min: %f%%",&a,&b);
			stats.r_hit = a * 1000;
		}
		/* Byte Hit Ratios:        5min: 96.6%, 60min: 96.6% */
		if(strstr(line,"Byte Hit Ratios") != NULL){
			float a,b;
			sscanf(line,"        Byte Hit Ratios:        5min: %f%%, 60min: %f%%",&a,&b);
			stats.b_hit = a * 1000 + b;
		}
		/* UP Time:        247256.904 seconds */
		if(strstr(line,"UP Time") != NULL){
			float a;
			sscanf(line,"        UP Time:        %f seconds",&a);
			stats.t_cpu = a * 1000;
		}
		/* CPU Time:       23487.042 seconds */
		if(strstr(line,"CPU Time") != NULL){
			float a;
			sscanf(line,"        CPU Time:       %f seconds",&a);
			stats.s_cpu = a * 1000;
		}
		line = strtok(NULL, "\n");
	}
	return 0;
}

void set_swift_record(struct module *mod, double st_array[],
		U_64 pre_array[], U_64 cur_array[], int inter)
{
	//qps
	if(cur_array[0] >= pre_array[0])
		st_array[0] = (cur_array[0] -  pre_array[0]) / inter;
	else
		st_array[0] = -1;
	//rt
	st_array[1] = cur_array[1]*1.0/1000;
	//r_hit b_hit
	st_array[2] = cur_array[2]*1.0/1000;
	st_array[3] = cur_array[3]*1.0/1000;
	//objs
	st_array[4] = cur_array[4];
	//in_bw out_bw
	if(cur_array[5] >= pre_array[5])
		st_array[5] = (cur_array[5] -  pre_array[5]) / inter;
	else
		st_array[5] = -1;
	if(cur_array[6] >= pre_array[6])
		st_array[6] = (cur_array[6] -  pre_array[6]) / inter;
	else
		st_array[6] = -1;
	//cpu
	if(cur_array[7] > pre_array[7] && cur_array[8] >= pre_array[8])
		st_array[7] = (cur_array[8] - pre_array[8]) * 100.0 / (cur_array[7] - pre_array[7]);
	else
		st_array[7] = -1;
}

int read_swift_stat(char *cmd)
{
	char msg[LEN_512];
	char buf[LEN_4096];
	sprintf(msg,
			"GET cache_object://localhost/%s "
			"HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Accept:*/*\r\n"
			"Connection: close\r\n\r\n",
			cmd);

	int len, conn, bytesWritten, fsize = 0;

	if(my_swift_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0){
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

	bytesWritten = mywrite_swift(conn, msg, strlen(msg));
	if (bytesWritten < 0) {
		close(conn);
		return -2;
	} else if (bytesWritten != strlen(msg)) {
		close(conn);
		return -3;
	}

	while ((len = myread_swift(conn, buf, sizeof(buf))) > 0) {
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

void read_swift_stats(struct module *mod, char *parameter)
{
	int retry = 0 ,pos = 0;
	char buf[LEN_1024];
	memset(&stats, 0, sizeof(stats));
	mgrport = atoi(parameter);
	if(!mgrport){
		mgrport = 81;
	}
	while(read_swift_stat("info") < 0 && retry < RETRY_NUM){
		retry++;
	}
	retry = 0;
	while(read_swift_stat("counters") < 0 && retry < RETRY_NUM){
		retry++;
	}
	pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld",
			stats.requests,
			stats.rt,
			stats.r_hit,
			stats.b_hit,
			stats.objs,
			stats.bytes_in,
			stats.bytes_out,
			stats.t_cpu,
			stats.s_cpu
		     );
	buf[pos] = '\0';
	set_mod_record(mod, buf);
}

void mod_register(struct module *mod)
{
	register_mod_fileds(mod, "--swift", swift_usage, swift_info, 9, read_swift_stats, set_swift_record);
}

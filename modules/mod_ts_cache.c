#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

/*
 * Structure for TS information
 */
struct stats_ts_cache {
	unsigned long long hit;
	unsigned long long ram_hit;
	unsigned long long band;
	unsigned long long n_hit;
	unsigned long long n_ram;
	unsigned long long n_ssd;
	unsigned long long ssd_hit;
};
//return value type
const static short int TS_REC_INT = 0;
const static short int TS_REC_COUNTER = 0;
const static short int TS_REC_FLOAT = 2;
const static short int TS_REC_STRING = 3;
//command type
const static short int TS_RECORD_GET = 3;
//records
const static int LINE_1024 = 1024;
const static int LINE_4096 = 4096;
const static char *RECORDS_NAME[]= {
	"proxy.node.cache_hit_ratio_avg_10s",
	"proxy.node.cache_hit_mem_ratio_avg_10s",
	"proxy.node.bandwidth_hit_ratio_avg_10s",
	"proxy.process.cache.read.success",
	"proxy.process.cache.ram.read.success",
	"proxy.process.cache.ssd.read.success"
};
//socket patch
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";
//const static char *sock_path = "/usr/local/var/trafficserver/mgmtapisocket";

static char *ts_cache_usage = "    --ts_cache          trafficserver cache statistics";

static struct mod_info ts_cache_info[] = {
	{"   hit", DETAIL_BIT, 0, STATS_NULL},
	{"ramhit", DETAIL_BIT, 0, STATS_NULL},
	{"  band", DETAIL_BIT, 0, STATS_NULL},
	{" n_hit", HIDE_BIT, 0, STATS_NULL},
	{" n_ram", HIDE_BIT, 0, STATS_NULL},
	{" n_ssd", HIDE_BIT, 0, STATS_NULL},
	{"ssdhit", DETAIL_BIT, 0, STATS_NULL}
};

void read_ts_cache_stats(struct module *mod)
{
	int fd = -1;
	struct sockaddr_un un;
	struct stats_ts_cache st_ts;
	int pos;
	char buf[LINE_4096];
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto done;
	}
	bzero(&st_ts, sizeof(st_ts));
	bzero(&un, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, sock_path);
	if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
		goto done;
	}

	int record_len = sizeof(RECORDS_NAME)/sizeof(RECORDS_NAME[0]);
	int i;
	const char *info;
	for ( i = 0; i < record_len; ++i) {
		info = RECORDS_NAME[i];
		long int info_len = strlen(info);
		short int command = TS_RECORD_GET;
		char write_buf[LINE_1024];
		*((short int *)&write_buf[0]) = command;
		*((long int *)&write_buf[2]) = info_len;
		strcpy(write_buf+6, info);
		write(fd, write_buf, 2+4+strlen(info));

		short int ret_status;
		short int ret_type;
		int read_len = read(fd, buf, LINE_1024);

		if (read_len != -1) {
			ret_status = *((short int *)&buf[0]);
			ret_type = *((short int *)&buf[6]);

			if (0 == ret_status) {
				if (ret_type < 2) {
					((unsigned long long *)&st_ts)[i] = *((long int *)&buf[8]);
				} 
				else if (2 == ret_type) {
					float ret_val_float = *((float *)&buf[8]);
					((unsigned long long *)&st_ts)[i] = (int)(ret_val_float * 1000);
				}
			}
		}
	}
done:
	if (-1 != fd)
		close(fd);
	pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,0",
			st_ts.hit,
			st_ts.ram_hit,
			st_ts.band,
			st_ts.n_hit,
			st_ts.n_ram,
			st_ts.n_ssd
		     );

	buf[pos] = '\0';
	set_mod_record(mod, buf);
}


void set_ts_cache_record(struct module *mod, double st_array[],
		U_64 pre_array[], U_64 cur_array[], int inter)
{
	st_array[0] = cur_array[0]/10.0;
	st_array[2] = cur_array[2]/10.0;

	// not ssd and sas
	if (cur_array[5] == 0 && cur_array[1]) {
		st_array[1] = cur_array[1]/10.0;
	}
	else {
		if (cur_array[3] > pre_array[3]) {
			st_array[1] = (cur_array[4] - pre_array[4]) * 100.0 / (cur_array[3] - pre_array[3]); 
			st_array[6] = (cur_array[5] - pre_array[5]) * 100.0 / (cur_array[3] - pre_array[3]); 
		}
	}
}



void mod_register(struct module *mod)
{
	register_mod_fileds(mod, "--ts_cache", ts_cache_usage, ts_cache_info, 7, read_ts_cache_stats, set_ts_cache_record);
}

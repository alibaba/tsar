#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tsar.h"

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
  "proxy.process.http.current_client_connections",
  "proxy.process.http.current_server_connections",
  "proxy.process.http.current_cache_connections",
  "proxy.process.net.connections_currently_open",
  "proxy.process.http.current_active_client_connections",
  "proxy.process.http.current_client_transactions",
  "proxy.process.http.current_server_transactions"
};
//socket patch
const static char *sock_path = "/var/run/trafficserver/mgmtapisocket";
//const static char *sock_path = "/usr/local/var/trafficserver/mgmtapisocket";

/*
 * Structure for TS information
 */
struct stats_ts_conn {
  //ts conn
  unsigned long long c_client;
  unsigned long long c_server;
  unsigned long long c_cache;
  unsigned long long c_open;
  unsigned long long c_a_client;
  unsigned long long t_client;
  unsigned long long t_server;
};

static char *ts_conn_usage = "    --ts                trafficserver connection statistics";

static struct mod_info ts_conn_info[] = {
  {"client", DETAIL_BIT, 0, STATS_NULL},
  {"server", DETAIL_BIT, 0, STATS_NULL},
  {" cache", DETAIL_BIT, 0, STATS_NULL},
  {"  open", DETAIL_BIT, 0, STATS_NULL},
  {" c_act", DETAIL_BIT, 0, STATS_NULL},
  {" t_cli", DETAIL_BIT, 0, STATS_NULL},
  {" t_srv", DETAIL_BIT, 0, STATS_NULL},
};

void set_ts_conn_record(struct module *mod, double st_array[],
                             U_64 pre_array[], U_64 cur_array[], int inter)
{
  int i;
  for (i = 0; i < 7; i++) {
    st_array[i] = cur_array[i];
  }
}

void read_ts_conn_stats(struct module *mod)
{
  int fd = -1;
  struct sockaddr_un un;
  struct stats_ts_conn st_ts;
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
    long ret_val;
    int read_len = read(fd, buf, LINE_1024);
    if (read_len != -1) {
      ret_status = *((short int *)&buf[0]);
      ret_type = *((short int *)&buf[6]);
    }
    if (0 == ret_status) {
      if (ret_type < 2) {
	ret_val= *((long int *)&buf[8]);
      } else if (2 == ret_type) {
	float ret_val_float = *((float *)&buf[8]);
        ret_val_float *= 100;
        ret_val = (unsigned long long)ret_val_float;
      } else {
        goto done;
      }
    }
    ((unsigned long long *)&st_ts)[i] = ret_val;
  }
done:
  if (-1 != fd)
    close(fd);
  pos = sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld",
                    st_ts.c_client,
                    st_ts.c_server,
		    st_ts.c_cache,
                    st_ts.c_open,
                    st_ts.c_a_client,
                    st_ts.t_client,
                    st_ts.t_server
                );
  buf[pos] = '\0';
  set_mod_record(mod, buf);
}

void mod_register(struct module *mod)
{
  register_mod_fileds(mod, "--ts_conn", ts_conn_usage, ts_conn_info, 7, read_ts_conn_stats, set_ts_conn_record);
}

#define   _GNU_SOURCE
#include "tsar.h"

#define LVS_PROC_STATS 	"/proc/net/ip_vs_stats"
#define LVS_CONN_PROC_STATS	"/proc/net/ip_vs_conn_stats"

#define LVS_CMD			"sudo /usr/local/sbin/slb_admin -ln --total --dump"
#define LVS_CONN_CMD	"sudo /usr/local/sbin/appctl -csa"

#define LVS_CMD_PATH	"/usr/local/sbin/slb_admin"
#define LVS_STORE_FMT(d)            \
	"%lld"d"%lld"d"%lld"d"%lld"d"%lld"d"%lld"d\
	"%lld"d"%lld"d"%lld"d"%lld"d"%lld"d"%lld"d"%lld"d"%lld"
#define MAX_LINE_LEN 1024
struct stats_lvs{
	unsigned long long stat;
	unsigned long long conns;
	unsigned long long pktin;
	unsigned long long pktout;
	unsigned long long bytin;
	unsigned long long bytout;

	unsigned long long total_conns;		/* total conns */
	unsigned long long local_conns;		/* local conns */
	unsigned long long act_conns;		/* active conns */
	unsigned long long inact_conns;		/* inactive conns */
	unsigned long long sync_conns;		/* sync conns */
	unsigned long long sync_act_conns;	/* sync active conns */
	unsigned long long sync_inact_conns;	/* sync inactive conns */
	unsigned long long template_conns;	/* template conns */
};
#define STATS_LVS_SIZE       (sizeof(struct stats_lvs))

static struct mod_info info[] = {
	{"  stat", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{" conns", SUMMARY_BIT,  MERGE_NULL,  STATS_SUB_INTER},
	{" pktin", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
	{"pktout", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
	{" bytin", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
	{"bytout", DETAIL_BIT,  MERGE_NULL,  STATS_SUB_INTER},
	{" total", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{" local", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{"  lact", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{"linact", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{"  sync", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{"  sact", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{"sinact", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
	{" templ", DETAIL_BIT,  MERGE_NULL,  STATS_NULL},
};

static char *lvs_usage = "    --lvs               lvs connections and packets and bytes in/out";
struct stats_lvs st_lvs;
/*
 *******************************************************
 * Read swapping statistics from ip_vs_stat
 *******************************************************
 */
static void
read_lvs_stat(int has_netframe)
{
	int    i = 0, use_popen = 0;
	char   tmp[5][16];
	FILE  *fp = NULL;
	char   line[MAX_LINE_LEN];

	if (has_netframe) {
		fp = popen(LVS_CMD, "r");
		use_popen = 1;
	} else if (!access(LVS_PROC_STATS, F_OK))
		fp = fopen(LVS_PROC_STATS, "r");

	if (!fp)
		return;

	st_lvs.stat = 1;
	while (fgets(line, MAX_LINE_LEN, fp) != NULL) {
		i++;
		if (i < 3) {
			continue;
		}
		if (!strncmp(line, "CPU", 3)) {
			/* CPU 0:       5462458943          44712664864          54084995692        8542115117674       41738811918899 */
			int    k = 0;
			k = strcspn(line, ":");
			sscanf(line + k + 1, "%s %s %s %s %s", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4]);
			st_lvs.conns += strtoll(tmp[0], NULL, 10);
			st_lvs.pktin += strtoll(tmp[1], NULL, 10);
			st_lvs.pktout += strtoll(tmp[2], NULL, 10);
			st_lvs.bytin += strtoll(tmp[3], NULL, 10);
			st_lvs.bytout += strtoll(tmp[4], NULL, 10);
		} else {
			/* 218EEA1A 1B3BA96D        0    163142140FA1F                0 */
			sscanf(line, "%s %s %s %s %s", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4]);
			st_lvs.conns += strtoll(tmp[0], NULL ,16);
			st_lvs.pktin += strtoll(tmp[1], NULL, 16);
			st_lvs.pktout += strtoll(tmp[2], NULL, 16);
			st_lvs.bytin += strtoll(tmp[3], NULL, 16);
			st_lvs.bytout += strtoll(tmp[4], NULL, 16);
			break;
		}
	}

	if (use_popen)
		pclose(fp);
	else
		fclose(fp);
}

static void
read_lvs_conn_stat(int has_netframe)
{
	int use_popen = 0;
	FILE *fp = NULL;
	char line[MAX_LINE_LEN];

	if (has_netframe) {
		fp = popen(LVS_CONN_CMD, "r");
		use_popen = 1;
	} else if (!access(LVS_CONN_PROC_STATS, F_OK))
		fp = fopen(LVS_CONN_PROC_STATS, "r");

	if (!fp)
		return;

	while (fgets(line, MAX_LINE_LEN, fp) != NULL) {

		if (has_netframe) {
			unsigned long long *item = NULL;

			/* Example:
			total_conns                              :  0
			local_conns                              :  0
			local_active_conns                       :  0
			local_inactive_conns                     :  0
			sync_conns                               :  0
			sync_active_conns                        :  0
			sync_inactive_conns                      :  0
			template_conns                           :  0
			*/
			if (!strncmp(line, "total_conns", 11))
				item = &st_lvs.total_conns;
			else if (!strncmp(line, "local_conns", 11))
				item = &st_lvs.local_conns;
			else if (!strncmp(line, "local_active_conns", 18))
				item = &st_lvs.act_conns;
			else if (!strncmp(line, "local_inactive_conns", 20))
				item = &st_lvs.inact_conns;
			else if (!strncmp(line, "sync_conns", 10))
				item = &st_lvs.sync_conns;
			else if (!strncmp(line, "sync_active_conns", 16))
				item = &st_lvs.sync_act_conns;
			else if (!strncmp(line, "sync_inactive_conns", 18))
				item = &st_lvs.sync_inact_conns;
			else if (!strncmp(line, "template_conns", 10))
				item = &st_lvs.template_conns;

			if (item)
				*item = strtoll(strchr(line, ':') + 1, NULL, 10);

		} else {
			/* Example:
			PersistConns: 17

			Total     Local     Sync      LocAct    SynAct    LocInAct  SynInAct  Flush     Flush
			Conns     Conns     Conns     Conns     Conns     Conns     Conns     Threshold State
			CPU0 : 58094     14710     43384     9522      28133     5188      15251     0         N/A
			CPU11: 49888     12589     37299     7657      22498     4932      14801     0         N/A
			TOTAL: 603642    151514    452128    90610     269578    60904     182550    0         N/A
			*/
			if (!strncmp(line, "PersistConns", 12)) {
				st_lvs.template_conns = strtoll(strchr(line, ':') + 1, NULL, 10);
			} else if (!strncmp(line, "TOTAL", 5)) {
				char tmp[7][16];

				sscanf(strchr(line, ':') + 1, "%s %s %s %s %s %s %s",
						tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6]);

				st_lvs.total_conns = strtoll(tmp[0], NULL, 10);
				st_lvs.local_conns = strtoll(tmp[1], NULL, 10);
				st_lvs.sync_conns = strtoll(tmp[2], NULL, 10);
				st_lvs.act_conns = strtoll(tmp[3], NULL, 10);
				st_lvs.sync_act_conns = strtoll(tmp[4], NULL, 10);
				st_lvs.inact_conns = strtoll(tmp[5], NULL, 10);
				st_lvs.sync_inact_conns = strtoll(tmp[6], NULL, 10);
			}
		}
	}

	if (use_popen)
		pclose(fp);
	else
		fclose(fp);
}

static void
read_lvs(struct module *mod)
{
	char buf[512] = { 0 };
	int pos = 0, has_netframe = 0;

	memset(&st_lvs, 0, sizeof(struct stats_lvs));

	if (!access(LVS_CMD_PATH, F_OK | X_OK))
		has_netframe = 1;

	read_lvs_stat(has_netframe);

	read_lvs_conn_stat(has_netframe);

	if (st_lvs.stat == 1) {
		pos = sprintf(buf, LVS_STORE_FMT(DATA_SPLIT),
				st_lvs.stat, st_lvs.conns,
				st_lvs.pktin, st_lvs.pktout, st_lvs.bytin, st_lvs.bytout,
				st_lvs.total_conns, st_lvs.local_conns, st_lvs.act_conns, st_lvs.inact_conns,
				st_lvs.sync_conns, st_lvs.sync_act_conns, st_lvs.sync_inact_conns,
				st_lvs.template_conns);
	} else {
		return;
	}

	buf[pos] = '\0';
	set_mod_record(mod, buf);
	return;
}


void
mod_register(struct module *mod)
{
	register_mod_fields(mod, "--lvs", lvs_usage, info, sizeof(info)/sizeof(struct mod_info), read_lvs, NULL);
}

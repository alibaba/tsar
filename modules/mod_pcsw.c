#include "tsar.h"


char *pcsw_usage = "    --pcsw              Process (task) creation and context switch";

struct stats_pcsw {
    unsigned long long context_switch;
    unsigned long processes;
};

#define STATS_PCSW_SIZE sizeof(struct pcsw_stats)

enum {PROC, CSWCH};


/*
 ***************************************************************************
 * Read processes (tasks) creation and context switches statistics from
 * /proc/stat
 *
 ***************************************************************************
 */
void
read_stat_pcsw(struct module *mod)
{
    FILE              *fp;
    char               line[LEN_4096];
    char               buf[LEN_4096];
    struct stats_pcsw  st_pcsw;

    memset(buf, 0, LEN_4096);
    memset(&st_pcsw, 0, sizeof(struct stats_pcsw));

    if ((fp = fopen(STAT, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_4096, fp) != NULL) {

        if (!strncmp(line, "ctxt ", 5)) {
            /* Read number of context switches */
            sscanf(line + 5, "%llu", &st_pcsw.context_switch);
        }

        else if (!strncmp(line, "processes ", 10)) {
            /* Read number of processes created since system boot */
            sscanf(line + 10, "%lu", &st_pcsw.processes);
        }
    }
    int pos = sprintf(buf, "%lld,%ld",
            st_pcsw.context_switch,
            st_pcsw.processes);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    fclose(fp);
}

static struct mod_info pcsw_info[] = {
    {" cswch", DETAIL_BIT,  0,  STATS_SUB_INTER},
    {"  proc", DETAIL_BIT,  0,  STATS_SUB_INTER}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--pcsw", pcsw_usage, pcsw_info, 2, read_stat_pcsw, NULL);
}


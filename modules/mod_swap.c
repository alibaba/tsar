#include "tsar.h"

struct stats_swap{
	unsigned long pswpin;
        unsigned long pswpout;
};


#define STATS_SWAP_SIZE       (sizeof(struct stats_swap))

static char *swap_usage = "    --swap              swap usage";

/*
*********************************************
* Read swapping statistics from /proc/vmstat.
*********************************************
*/
static void read_vmstat_swap(struct module *mod)
{
        FILE *fp;
        char line[4096], buf[LEN_4096];
	memset(buf, 0, LEN_4096);
	struct stats_swap st_swap;
	memset(&st_swap, 0, sizeof(struct stats_swap));
        if ((fp = fopen(VMSTAT, "r")) == NULL) {
                return ;
	}

        while (fgets(line, LEN_4096, fp) != NULL) {

                if (!strncmp(line, "pswpin ", 7)) {
                        /* Read number of swap pages brought in */
                        sscanf(line + 7, "%lu", &st_swap.pswpin);
                }
                else if (!strncmp(line, "pswpout ", 8)) {
                        /* Read number of swap pages brought out */
                        sscanf(line + 8, "%lu", &st_swap.pswpout);
                }
        }
        fclose(fp);
	int pos = sprintf(buf, "%ld,%ld", st_swap.pswpin, st_swap.pswpout);
	buf[pos] = '\0';
	set_mod_record(mod, buf);
	return;
}
static struct mod_info swap_info[] = {
	{" swpin", DETAIL_BIT,  0,  STATS_SUB_INTER},
        {"swpout", DETAIL_BIT,  0,  STATS_SUB_INTER}
};

void mod_register(struct module *mod)
{
	register_mod_fileds(mod, "--swap", swap_usage, swap_info, 2, read_vmstat_swap, NULL);
}

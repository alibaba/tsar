#include "tsar.h"

struct stats_swap {
    unsigned long long pswpin;
    unsigned long long pswpout;
    unsigned long long swaptotal;
    unsigned long long swapfree;
};


#define STATS_SWAP_SIZE       (sizeof(struct stats_swap))

static char *swap_usage = "    --swap              swap usage";

/*
 *************************************************************
 * Read swapping statistics from /proc/vmstat & /proc/meminfo.
 *************************************************************
 */
static void
read_vmstat_swap(struct module *mod)
{
    FILE              *fp;
    char               line[LEN_4096], buf[LEN_4096];
    struct stats_swap  st_swap;

    memset(buf, 0, LEN_4096);
    memset(&st_swap, 0, sizeof(struct stats_swap));
    /* read /proc/vmstat*/
    if ((fp = fopen(VMSTAT, "r")) == NULL) {
        return ;
    }

    while (fgets(line, LEN_4096, fp) != NULL) {

        if (!strncmp(line, "pswpin ", 7)) {
            /* Read number of swap pages brought in */
            sscanf(line + 7, "%llu", &st_swap.pswpin);

        } else if (!strncmp(line, "pswpout ", 8)) {
            /* Read number of swap pages brought out */
            sscanf(line + 8, "%llu", &st_swap.pswpout);
        }
    }
    fclose(fp);
    /* read /proc/meminfo */
    if ((fp = fopen(MEMINFO, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_4096, fp) != NULL) {
        if (!strncmp(line, "SwapTotal:", 10)) {
	    sscanf(line + 10, "%llu", &st_swap.swaptotal);

	} else if (!strncmp(line, "SwapFree:", 9)) {
	    sscanf(line + 9, "%llu", &st_swap.swapfree);
	}
    }
    fclose(fp);

    sprintf(buf, "%lld,%lld,%lld,%lld", st_swap.pswpin, st_swap.pswpout, st_swap.swaptotal*1024, st_swap.swapfree*1024);
    set_mod_record(mod, buf);
    return;
}

static void
set_swap_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    if (cur_array[0] >= pre_array[0]) {
        st_array[0] = (cur_array[0] -  pre_array[0]) / inter;

    } else {
        st_array[0] = 0;
    }

    if (cur_array[1] >= pre_array[1]) {
        st_array[1] = (cur_array[1] -  pre_array[1]) / inter;

    } else {
        st_array[1] = 0;
    }

    /* calc total swap and use util */
    st_array[2] = cur_array[2];
    st_array[3] = 100.0 - cur_array[3] * 100.0 / cur_array[2];
}

static struct mod_info swap_info[] = {
    {" swpin", DETAIL_BIT,  0,  STATS_NULL},
    {"swpout", DETAIL_BIT,  0,  STATS_NULL},
    {" total", DETAIL_BIT,  0,  STATS_NULL},
    {"  util", DETAIL_BIT,  0,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--swap", swap_usage, swap_info, 4, read_vmstat_swap, set_swap_record);
}

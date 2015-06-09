#include "tsar.h"

char *mem_usage =
"    --mem               Physical memory share (active, inactive, cached, free, wired)";

/* Structure for memory and swap space utilization statistics */
struct stats_mem {
    unsigned long frmkb;
    unsigned long bufkb;
    unsigned long camkb;
    unsigned long tlmkb;
    unsigned long acmkb;
    unsigned long iamkb;
    unsigned long slmkb;
    unsigned long frskb;
    unsigned long tlskb;
    unsigned long caskb;
    unsigned long comkb;
};

static void
read_mem_stats(struct module *mod)
{
    FILE             *fp;
    char              line[LEN_128];
    char              buf[LEN_4096];
    struct stats_mem  st_mem;

    memset(buf, 0, LEN_4096);
    memset(&st_mem, 0, sizeof(struct stats_mem));
    if ((fp = fopen(MEMINFO, "r")) == NULL) {
        return;
    }

    while (fgets(line, 128, fp) != NULL) {

        if (!strncmp(line, "MemTotal:", 9)) {
            /* Read the total amount of memory in kB */
            sscanf(line + 9, "%lu", &st_mem.tlmkb);
        }
        else if (!strncmp(line, "MemFree:", 8)) {
            /* Read the amount of free memory in kB */
            sscanf(line + 8, "%lu", &st_mem.frmkb);
        }
        else if (!strncmp(line, "Buffers:", 8)) {
            /* Read the amount of buffered memory in kB */
            sscanf(line + 8, "%lu", &st_mem.bufkb);
        }
        else if (!strncmp(line, "Cached:", 7)) {
            /* Read the amount of cached memory in kB */
            sscanf(line + 7, "%lu", &st_mem.camkb);
        }
        else if (!strncmp(line, "Active:", 7)) {
            /* Read the amount of Active memory in kB */
            sscanf(line + 7, "%lu", &st_mem.acmkb);
        }
        else if (!strncmp(line, "Inactive:", 9)) {
            /* Read the amount of Inactive memory in kB */
            sscanf(line + 9, "%lu", &st_mem.iamkb);
        }
        else if (!strncmp(line, "Slab:", 5)) {
            /* Read the amount of Slab memory in kB */
            sscanf(line + 5, "%lu", &st_mem.slmkb);
        }
        else if (!strncmp(line, "SwapCached:", 11)) {
            /* Read the amount of cached swap in kB */
            sscanf(line + 11, "%lu", &st_mem.caskb);
        }
        else if (!strncmp(line, "SwapTotal:", 10)) {
            /* Read the total amount of swap memory in kB */
            sscanf(line + 10, "%lu", &st_mem.tlskb);
        }
        else if (!strncmp(line, "SwapFree:", 9)) {
            /* Read the amount of free swap memory in kB */
            sscanf(line + 9, "%lu", &st_mem.frskb);
        }
        else if (!strncmp(line, "Committed_AS:", 13)) {
            /* Read the amount of commited memory in kB */
            sscanf(line + 13, "%lu", &st_mem.comkb);
        }
    }

    int pos = sprintf(buf, "%lu,%u,%lu,%lu,%lu,%u",
            st_mem.frmkb,
            0, /* used */
            st_mem.bufkb,
            st_mem.camkb,
            st_mem.tlmkb,
            0 /* util */);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    fclose(fp);
    return;
}
static void
set_mem_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    if ((cur_array[4] - cur_array[0] -cur_array[2] -cur_array[3]) < 0) {
        for (i = 0; i < 6; i++) 
            st_array[i] = -1;
        return;
    }
    st_array[0] = cur_array[0]<<10;
    st_array[1] = (cur_array[4] - cur_array[0] -cur_array[2] -cur_array[3])<<10;
    st_array[2] = cur_array[2]<<10;
    st_array[3] = cur_array[3]<<10;
    st_array[4] = cur_array[4]<<10;
    st_array[5] = st_array[1] * 100.0 / (cur_array[4]<<10);
}

static struct mod_info mem_info[] = {
    {"  free", DETAIL_BIT,  0,  STATS_NULL},
    {"  used", DETAIL_BIT,  0,  STATS_NULL},
    {"  buff", DETAIL_BIT, 0,  STATS_NULL},
    {"  cach", DETAIL_BIT,  0,  STATS_NULL},
    {" total", DETAIL_BIT,  0,  STATS_NULL},
    {"  util", SUMMARY_BIT,  0,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--mem", mem_usage, mem_info, 6, read_mem_stats, set_mem_record);
}

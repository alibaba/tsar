#include "public.h"

#define PAGING_DETAIL_HDR(d)                \
    "  pgin"d" pgout"d" fault"d"majflt"d        \
"  free"d" scank"d" scand"d" steal"

#define PAGING_STORE_FMT(d)         \
    "%ld"d"%ld"d"%ld"d"%ld"d        \
"%ld"d"%ld"d"%ld"d"%ld"

#define PAGING_DETAIL_FMT(d)        \
    "%5.1f"d"%5.1f"d"%5.1f"d"%5.1f"d    \
"%5.1f"d"%5.1f"d"%5.1f"d"%5.1f"

#define PAGING_SUMMARY_HDR(d)           \
    "  pgin"d" pgout"

char *paging_usage = "    --paging            Paging statistics";

/* Structure for paging statistics */
struct stats_paging {
    unsigned long pgpgin;
    unsigned long pgpgout;
    unsigned long pgfault;
    unsigned long pgmajfault;
    unsigned long pgfree;
    unsigned long pgscan_kswapd;
    unsigned long pgscan_direct;
    unsigned long pgsteal;
}s_st_paging[2];

#define PAGING_STRING_OPS(str, ops, fmt, stat, d)   \
    ops(str, fmt,                   \
            stat d pgpgin,              \
            stat d pgpgout,             \
            stat d pgfault,             \
            stat d pgmajfault,              \
            stat d pgfree,              \
            stat d pgscan_kswapd,           \
            stat d pgscan_direct,           \
            stat d pgsteal)

#define STATS_PAGING_SIZE (sizeof(struct stats_paging))

union paging_statistics {
    struct paging_detail_statistics {
        double in;
        double out;
        double fault;
        double majfault;
        double free;
        double kswapd;
        double direct;
        double steal;
    } paging_detail, paging_summary;
} paging_statis[NR_ARRAY];


/*
 *******************************************
 *
 * Read paging statistics from /proc/vmstat.
 *
 *******************************************
 */
void
read_vmstat_paging(struct module *mod, int data_type)
{
    int            ok = FALSE;
    FILE          *fp;
    char           line[128], buf[MAX_LINE_LEN];
    unsigned long  pgtmp;

    if ((fp = fopen(VMSTAT, "r")) == NULL) {
        return;
    }
    myalloc(st_paging, stats_paging, STATS_PAGING_SIZE);
    st_paging->pgsteal = 0;
    st_paging->pgscan_kswapd = st_paging->pgscan_direct = 0;

    while (fgets(line, 128, fp) != NULL) {

        if (!strncmp(line, "pgpgin ", 7)) {
            /* Read number of pages the system paged in */
            sscanf(line + 7, "%lu", &st_paging->pgpgin);
            ok = TRUE;
        }

        else if (!strncmp(line, "pgpgout ", 8)) {
            /* Read number of pages the system paged out */
            sscanf(line + 8, "%lu", &st_paging->pgpgout);
        }

        else if (!strncmp(line, "pgfault ", 8)) {
            /* Read number of faults (major+minor) made by the system */
            sscanf(line + 8, "%lu", &st_paging->pgfault);
        }

        else if (!strncmp(line, "pgmajfault ", 11)) {
            /* Read number of faults (major only) made by the system */
            sscanf(line + 11, "%lu", &st_paging->pgmajfault);
        }

        else if (!strncmp(line, "pgfree ", 7)) {
            /* Read number of pages freed by the system */
            sscanf(line + 7, "%lu", &st_paging->pgfree);
        }

        else if (!strncmp(line, "pgsteal_", 8)) {
            /* Read number of pages stolen by the system */
            sscanf(strchr(line, ' '), "%lu", &pgtmp);
            st_paging->pgsteal += pgtmp;
        }

        else if (!strncmp(line, "pgscan_kswapd_", 14)) {
            /* Read number of pages scanned by the kswapd daemon */
            sscanf(strchr(line, ' '), "%lu", &pgtmp);
            st_paging->pgscan_kswapd += pgtmp;
        }

        else if (!strncmp(line, "pgscan_direct_", 14)) {
            /* Read number of pages scanned directly */
            sscanf(strchr(line, ' '), "%lu", &pgtmp);
            st_paging->pgscan_direct += pgtmp;
        }
    }
    int pos = PAGING_STRING_OPS(buf, sprintf,
            PAGING_STORE_FMT(DATA_SPLIT), st_paging, ->);
    buf[pos] = '\0';
    mod->detail = strdup(buf);

    fclose(fp);
    return;
}

char *
paging_ops(char *last_record,
    char *curr_record,
    time_t last_time,
    time_t curr_time,
    int data_type,
    int output_type)
{
    int            pos = 0;
    char           buf[MAX_STRING_LEN];
    unsigned long  itv;

    PAGING_STRING_OPS(last_record, sscanf,
            PAGING_STORE_FMT(DATA_SPLIT), &s_st_paging[1], .);
    PAGING_STRING_OPS(curr_record, sscanf,
            PAGING_STORE_FMT(DATA_SPLIT), &s_st_paging[0], .);

    DECLARE_TMP_MOD_STATISTICS(paging);

    if (data_type == DATA_DETAIL || data_type == DATA_SUMMARY) {
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgpgin, itv, in);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgpgout, itv, out);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgfault, itv, fault);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgmajfault, itv, majfault);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgfree, itv, free);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgscan_kswapd, itv, kswapd);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgscan_direct, itv, direct);
        COMPUTE_MOD_VALUE(paging, S_VALUE, detail, pgsteal, itv, steal);

        SET_MOD_STATISTICS(paging, in, itv, detail);
        SET_MOD_STATISTICS(paging, out, itv, detail);
        SET_MOD_STATISTICS(paging, fault, itv, detail);
        SET_MOD_STATISTICS(paging, majfault, itv, detail);
        SET_MOD_STATISTICS(paging, free, itv, detail);
        SET_MOD_STATISTICS(paging, kswapd, itv, detail);
        SET_MOD_STATISTICS(paging, direct, itv, detail);
        SET_MOD_STATISTICS(paging, steal, itv, detail);
    }
    if (data_type == DATA_DETAIL) {
        PRINT(buf + pos, paging_tmp_s.paging_detail.in, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.out, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.fault, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.majfault, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.free, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.kswapd, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.direct, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_detail.steal, pos, output_type);
    }

    else if (data_type == DATA_SUMMARY) {
        PRINT(buf + pos, paging_tmp_s.paging_summary.in, pos, output_type);
        PRINT(buf + pos, paging_tmp_s.paging_summary.out, pos, output_type);
    }
    buf[pos - 1] = '\0';
    return(strdup(buf));
}

static char **
get_paging_avg(int data_type, int count)
{
    int    i;
    int    pos[3] = {0, 0, 0};
    char **statis;
    INIT_STRING_P(statis, 3, MAX_STRING_LEN);
    for(i = 0; i < 3; i++) {
        if (data_type == DATA_DETAIL) {
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.in, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.out, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.fault, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.majfault, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.free, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.kswapd, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.direct, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_detail.steal, i, count, OUTPUT_PRINT);
        }

        else if(data_type == DATA_SUMMARY)  {
            __PRINT_AVG(statis, pos, paging_statis, paging_summary.in, i, count, OUTPUT_PRINT);
            __PRINT_AVG(statis, pos, paging_statis, paging_summary.out, i, count, OUTPUT_PRINT);
        }

    }
    return statis;
}


void
mod_register(struct module *mod)
{
    sprintf(mod->detail_hdr, PAGING_DETAIL_HDR(" "));
    sprintf(mod->summary_hdr, PAGING_SUMMARY_HDR(" "));
    mod->usage = paging_usage;
    sprintf(mod->opt_line, "--paging");
    mod->data_collect = read_vmstat_paging;
    mod->data_operation = paging_ops;
    mod->show_avg = get_paging_avg;
    mod->mod_free = func_mod_free;
}

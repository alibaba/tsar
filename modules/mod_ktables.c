#include "public.h"

#define KTABLES_DETAIL_HDR(d)           \
    " fused"d" iused"d" dstat"d" pytnr"

#define KTABLES_STORE_FMT(d)            \
    "%d"d"%d"d"%d"d"%d"

#define KTABLES_DETAIL_FMT(d)           \
    "%d"d"%d"d"%d"d"%d"

#define KTABLES_SUMMARY_FMT         \
    KTABLES_DETAIL_FMT

#define KTABLES_SUMMARY_HDR         \
    KTABLES_DETAIL_HDR

char *ktables_usage = "    --ktables           Kernel table statistics";

/* Structure for kernel tables statistics */
struct stats_ktables {
    unsigned int  file_used;
    unsigned int  inode_used;
    unsigned int  dentry_stat;
    unsigned int  pty_nr;
} s_st_ktables[2];

#define STATS_KTABLES_SIZE      (sizeof(struct stats_ktables))

#define KTABLES_STRING_OPS(str, ops, fmt, stat, d)  \
    ops(str, fmt,                   \
            stat d file_used,               \
            stat d inode_used,              \
            stat d dentry_stat,             \
            stat d pty_nr)

union ktables_statistics {
    struct ktables_detail_statistics
    {
        double fused;
        double iused;
        double dstat;
        double ptynr;
    } ktables_detail, ktbales_summary;
} ktables_statis[NR_ARRAY];


/*
 *********************************************************
 *Read kernel tables statistics from various system files.
 *********************************************************
 */
void
read_kernel_tables(struct module *mod, int data_type)
{
    char          buf[MAX_LINE_LEN];
    FILE         *fp;
    unsigned int  parm;
    myalloc(st_ktables, stats_ktables, STATS_KTABLES_SIZE);

    /* Open /proc/sys/fs/dentry-state file */
    if ((fp = fopen(FDENTRY_STATE, "r")) != NULL) {
        fscanf(fp, "%*d %u", &st_ktables->dentry_stat);
        if (fclose(fp) < 0) {
            return;
        }
    }

    /* Open /proc/sys/fs/file-nr file */
    if ((fp = fopen(FFILE_NR, "r")) != NULL) {
        fscanf(fp, "%u %u", &st_ktables->file_used, &parm);
        if (fclose(fp) < 0) {
            return;
        }
        /*
         * The number of used handles is the number of allocated ones
         * minus the number of free ones.
         */
        st_ktables->file_used -= parm;
    }

    /* Open /proc/sys/fs/inode-state file */
    if ((fp = fopen(FINODE_STATE, "r")) != NULL) {
        fscanf(fp, "%u %u", &st_ktables->inode_used, &parm);
        if (fclose(fp) < 0) {
            return;
        }
        /*
         * The number of inuse inodes is the number of allocated ones
         * minus the number of free ones.
         */
        st_ktables->inode_used -= parm;
    }

    /* Open /proc/sys/kernel/pty/nr file */
    if ((fp = fopen(PTY_NR, "r")) != NULL) {
        fscanf(fp, "%u", &st_ktables->pty_nr);
        if (fclose(fp) < 0) {
            return;
        }
    }
    int pos = KTABLES_STRING_OPS(buf, sprintf,
                                 KTABLES_STORE_FMT(DATA_SPLIT), st_ktables, ->);
    buf[pos] = '\0';
    mod->detail = strdup(buf);
}

static char **
get_avg(int data_type, int count)
{
    int     i, pos[3] = {0, 0, 0};
    char  **s_ktables;
    INIT_STRING_P(s_ktables, 3, MAX_STRING_LEN);
    for (i = 0; i < 3; i++) {
        __PRINT_AVG(s_ktables, pos, ktables_statis, ktables_detail.fused, i, count, OUTPUT_PRINT);
        __PRINT_AVG(s_ktables, pos, ktables_statis, ktables_detail.iused, i, count, OUTPUT_PRINT);
        __PRINT_AVG(s_ktables, pos, ktables_statis, ktables_detail.dstat, i, count, OUTPUT_PRINT);
        __PRINT_AVG(s_ktables, pos, ktables_statis, ktables_detail.ptynr, i, count, OUTPUT_PRINT);
    }
    return s_ktables;
}

char *
ktables_ops(char *last_record, char *curr_record, time_t last_time,
    time_t curr_time, int data_type, int output_type)
{
    char           buf[MAX_STRING_LEN];
    unsigned long  itv;

    CALITV(last_time, curr_time, itv);

    KTABLES_STRING_OPS(last_record, sscanf,
            KTABLES_STORE_FMT(DATA_SPLIT), &s_st_ktables[1], .);

    KTABLES_STRING_OPS(curr_record, sscanf,
            KTABLES_STORE_FMT(DATA_SPLIT), &s_st_ktables[0], .);

    DECLARE_TMP_MOD_STATISTICS(ktables);

    if (data_type == DATA_DETAIL || data_type == DATA_SUMMARY) {
        SET_CURRENT_VALUE(ktables, detail, file_used, fused);
        SET_CURRENT_VALUE(ktables, detail, inode_used, iused);
        SET_CURRENT_VALUE(ktables, detail, dentry_stat, dstat);
        SET_CURRENT_VALUE(ktables, detail, pty_nr, ptynr);
    }

    if(data_type == DATA_DETAIL || data_type == DATA_SUMMARY) {
        SET_MOD_STATISTICS(ktables, fused, itv, detail);
        SET_MOD_STATISTICS(ktables, iused, itv, detail);
        SET_MOD_STATISTICS(ktables, dstat, itv, detail);
        SET_MOD_STATISTICS(ktables, ptynr, itv, detail);

        int pos = 0;
        PRINT(buf + pos, ktables_tmp_s.ktables_detail.fused, pos, output_type);
        PRINT(buf + pos, ktables_tmp_s.ktables_detail.iused, pos, output_type);
        PRINT(buf + pos, ktables_tmp_s.ktables_detail.dstat, pos, output_type);
        PRINT(buf + pos, ktables_tmp_s.ktables_detail.ptynr, pos, output_type);

        buf[pos - 1] = '\0';

    }

    return(strdup(buf));
}

void
mod_register(struct module *mod)
{
    sprintf(mod->detail_hdr, KTABLES_DETAIL_HDR(" "));
    sprintf(mod->summary_hdr, KTABLES_SUMMARY_HDR(" "));
    mod->usage = ktables_usage;
    sprintf(mod->opt_line, "--ktables");
    mod->data_collect = read_kernel_tables;
    mod->data_operation = ktables_ops;
    mod->show_avg = get_avg;
    mod->mod_free = func_mod_free;
}

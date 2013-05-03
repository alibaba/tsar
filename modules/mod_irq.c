#include "public.h"

#define IRQ_DETAIL_HDR "  intr"
#define IRQ_STORE_FMT(d) "%lld"
#define IRQ_SUMMARY_HDR "intr/s"

char * irq_usage = "    --irq               Interrupts statistics";

/*
 * Structure for interrupts statistics.
 * In activity buffer: First structure is for total number of interrupts ("SUM").
 * Following structures are for each individual interrupt (0, 1, etc.)
 *
 * NOTE: The total number of interrupts is saved as a %llu by the kernel,
 * whereas individual interrupts are saved as %u.
 */
struct __stats_irq {
    unsigned long long irq_nr;
};

typedef struct __stats_irq (*stats_irq)[2];

stats_irq s_st_irq;

#define STATS_IRQ_SIZE  (sizeof(struct __stats_irq))

union __irq_statistics {
    struct irq_detail_statistics {
        double intr;
    } irq_detail, irq_summary;
};

#define MAXIRQ 128

static union __irq_statistics irq_statistics[MAXIRQ][NR_ARRAY];

#define IRQ_STRING_OPS(str, ops, fmt, stat, d)  \
    ops(str, fmt, stat d irq_nr)


#define SET_IRQ_STATISTICS(val, target, member, i)  \
    __SET_MOD_STATISTICS                \
(val.irq_detail.member,             \
 target[MEAN].irq_detail.member,        \
 target[MAX].irq_detail.member,         \
 target[MIN].irq_detail.member,         \
 (i))

static int f_init = FALSE;

char word[12];              // reocrd the word read from textline
char textline[256];         //record a line of text
unsigned char  text_index = 0;  //the index of textline above

static void
init_structure(int nr)
{
    s_st_irq = (stats_irq)
        malloc(STATS_IRQ_SIZE * nr * 2);
    if (s_st_irq == NULL) {
        do_debug(LOG_FATAL, "not enough memory!\n");
    }
}


long
stringToNum(char *string)
{
    long  temp = 0;
    const char *ptr = string;

    while (*string != '\0') {
        if ((*string < '0') || (*string > '9')) {
            break;
        }
        temp = temp * 10 + (*string - '0');
        string++;
    }
    if (*ptr == '-') {
        temp = -temp;
    }
    return temp;
}

void
getword(char *textline)
{
    int i;

    i = 0;
    for (; isspace(textline[text_index]); text_index++) {
        ;
    }
    for (; !isspace(textline[text_index]); i++, text_index++) {
        word[i] = textline[text_index];
    }

    word[i]= '\0';
}

int
countCPUNumber()   /*the number of word is equal to the number of processor*/
{
    int in = 0;
    int cpu_number = 0;
    for (; textline[text_index] != '\0'; text_index++) {
        if (isalnum(textline[text_index]) && in == 0) {
            in = 1;
            cpu_number++;

        } else if (isspace(textline[text_index])) {
            in = 0;
        }
    }
    return cpu_number;
}

/*
 ***************************************************************************
 * Count number of interrupts that are in /proc/stat file.
 *
 * RETURNS:
 * Number of interrupts, including total number of interrupts.
 ***************************************************************************
 */
static int
count_irq_nr(char *record)
{
    int    n=0;
    int    sw = TRUE;
    char   line[256];
    char   i;
    FILE  *fp;
    if ((fp = fopen(INTERRUPT, "r")) == NULL) {
        return 0;
    }
    while (fgets(line, 256, fp) != NULL) {
        if (!sw) {
            sscanf(line, "%*c%*c%c:", &i);
            if (i >= '0' && i <= '9') {
                n++;

            } else {
                continue;
            }

        } else {
            sw = FALSE;
        }
    }
    if (fclose(fp) < 0) {
        exit(-1);
    }
    return n;
}

void
read_irq_stat(struct module *mod, int data_type)
{
    int                 i, pos = 0;
    int                 cpu_nr;
    int                 inter_num;
    char                buf[MAX_LINE_LEN];
    FILE               *fp;
    unsigned long long  inter_sum;

    fp = fopen(INTERRUPT, "r");

    if (fp == NULL) {
        perror("unable to open file /proc/interrupts");
        exit(-1);
    }

    if (!fgets(textline, 256, fp)) {
        if (fclose(fp) < 0) {
            exit(-1);
        }
        return;
    }

    cpu_nr = countCPUNumber();

    while (fgets(textline, MAX_LINE_LEN, fp) != NULL) {
        text_index = 0;
        getword(textline);
        if (isalpha(*word)) {
            continue;
        }
        inter_num = (int)stringToNum(word);
        inter_sum = 0;
        for (i = 0; i!= cpu_nr; i++) {
            getword(textline);
            inter_sum += stringToNum(word);
        }
        pos += sprintf(buf + pos, "in%d=%lld;", inter_num, inter_sum);
    }
    if (fclose(fp) < 0) {
        return;
    }
    buf[pos] = '\0';
    mod->detail = strdup(buf);
}


void
__irq_ops(char *_detail_last, char *_detail_curr, int dtype, int otype, stats_irq si,
    char *_buf, int *pos, unsigned long itv, unsigned int idx)
{
    int                     len = 0;
    union __irq_statistics  tmp;

    IRQ_STRING_OPS(_detail_curr, sscanf,
            IRQ_STORE_FMT(DATA_SPLIT), &(*si)[0], .);
    IRQ_STRING_OPS(_detail_last, sscanf,
            IRQ_STORE_FMT(DATA_SPLIT), &(*si)[1], .);

    __COMPUTE_MOD_VALUE(tmp.irq_detail.intr, S_VALUE,
            (*si)[1].irq_nr, (*si)[0].irq_nr, itv);

    SET_IRQ_STATISTICS(tmp, irq_statistics[idx], intr, itv);

    if (dtype == DATA_DETAIL) {
        /* write each record to buffer */
        PRINT(_buf + len, tmp.irq_detail.intr, len, otype);

    } else if (dtype == DATA_SUMMARY) {
        PRINT(_buf + len, tmp.irq_detail.intr, len, otype);
    }
    *pos += len - 1 ;
}


char *
irq_ops(char *last_record, char *curr_record, time_t last_time,
    time_t curr_time, int data_type, int output_type)
{
    int            pos = 0;
    char           buf[MAX_LINE_LEN];
    unsigned int   i = 0;
    unsigned long  itv;
    /* if statistic structure is not inited,
       we will alloc space here */
    int nr = count_irq_nr(last_record);
    if (!f_init) {
        init_structure(nr);
        f_init = TRUE;
    }

    CALITV(last_time, curr_time, itv);

    stats_irq temp_si = s_st_irq;

    char last_mnt[MAX_STRING_LEN] = {0};
    char curr_mnt[MAX_STRING_LEN] = {0};

    if (*last_record == '\0') {
        /* first record */
    } else {
        while ((last_record = getitem(last_record, last_mnt)) != NULL &&
                (curr_record = getitem(curr_record, curr_mnt)) != NULL)
        {

            __irq_ops(last_mnt , curr_mnt,
                    data_type, output_type,
                    temp_si, buf + pos, &pos, itv, i);
            i++;
            temp_si++;
            memset(last_mnt, '0', MAX_STRING_LEN);
            memset(curr_mnt, '0', MAX_STRING_LEN);
        }
    }

    buf[pos] = '\0';
    return(strdup(buf));
}

static char **
get_avg(int data_type, int count)
{
    int     i, j;
    int     pos[3] = {0, 0, 0};
    int     nr = count_irq_nr(NULL);
    char  **statis;

    INIT_STRING_P(statis, 3, MAX_STRING_LEN);

    for (j = 0; j < nr; j++) {
        for (i = 0; i < 3; i++) {
            if (data_type == DATA_DETAIL) {
                __PRINT_AVG(statis, pos, irq_statistics[j], irq_detail.intr, i, count, OUTPUT_PRINT);

            } else if (data_type == DATA_SUMMARY) {
                /* __PRINT_AVG(statis, pos, irq_statistics[j], irq_summary.kutil, i, count, OUTPUT_PRINT); */
            }
            pos[i] -= 1;
        }
    }
    return statis;
}

void
mod_register(struct module *mod)
{
    sprintf(mod->detail_hdr, IRQ_DETAIL_HDR);
    sprintf(mod->summary_hdr, IRQ_SUMMARY_HDR);
    mod->usage = irq_usage;
    sprintf(mod->opt_line, "--irq");
    mod->data_collect = read_irq_stat;
    mod->data_operation = irq_ops;
    mod->show_avg = get_avg;
    mod->mod_free = func_mod_free;
}

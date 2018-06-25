
/*
 * (C) 2010-2011 Alibaba Group Holding Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include "tsar.h"
#include <fnmatch.h>


/*
 * adjust print opt line
 */
void
adjust_print_opt_line(char *n_opt_line, const char *opt_line, int hdr_len)
{
    int    pad_len;
    char   pad[LEN_128] = {0};

    if (hdr_len > strlen(opt_line)) {
        pad_len = (hdr_len - strlen(opt_line)) / 2;

        memset(pad, '-', pad_len);
        strcat(n_opt_line, pad);
        strcat(n_opt_line, opt_line);
        memset(pad, '-', hdr_len - pad_len - strlen(opt_line));
        strcat(n_opt_line, pad);

    } else {
        strncat(n_opt_line, opt_line, hdr_len);
    }
}


/*
 * print header and update mod->n_item
 */
void
print_header()
{
    int    i;
    char   header[LEN_1M] = {0};
    char   opt_line[LEN_1M] = {0};
    char   hdr_line[LEN_1M] = {0};
    char   opt[LEN_256] = {0};
    char   n_opt[LEN_256] = {0};
    char   mod_hdr[LEN_256] = {0};
    char  *token, *s_token, *n_record;
    struct module *mod = NULL;

    if (conf.running_mode == RUN_PRINT_LIVE) {
        sprintf(opt_line, "Time             %s", PRINT_SEC_SPLIT);
        sprintf(hdr_line, "Time             %s", PRINT_SEC_SPLIT);

    } else {
        sprintf(opt_line, "Time          %s", PRINT_SEC_SPLIT);
        sprintf(hdr_line, "Time          %s", PRINT_SEC_SPLIT);
    }

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = mods[i];
        if (!mod->enable) {
            continue;
        }

        memset(n_opt, 0, sizeof(n_opt));
        memset(mod_hdr, 0, sizeof(mod_hdr));
        get_mod_hdr(mod_hdr, mod);

        if (strpbrk(mod->record, ITEM_SPLIT) && MERGE_NOT == conf.print_merge) {
            n_record = strdup(mod->record);
            /* set print opt line */
            token = strtok(n_record, ITEM_SPLIT);
            int count = 0;
            mod->p_item = -1;
            while (token) {
                s_token = strpbrk(token, ITEM_SPSTART);
                if (s_token) {
                    memset(opt, 0, sizeof(opt));
                    memset(n_opt, 0, sizeof(n_opt));
                    strncat(opt, token, s_token - token);
                    if (*mod->print_item != 0 && fnmatch(mod->print_item, opt, 0)) {
                        token = strtok(NULL, ITEM_SPLIT);
                        count++;
                        continue;
                    }
                    mod->p_item = count++;
                    adjust_print_opt_line(n_opt, opt, strlen(mod_hdr));
                    strcat(opt_line, n_opt);
                    strcat(opt_line, PRINT_SEC_SPLIT);
                    strcat(hdr_line, mod_hdr);
                    strcat(hdr_line, PRINT_SEC_SPLIT);
                }
                token = strtok(NULL, ITEM_SPLIT);
            }
            free(n_record);
            n_record = NULL;

        } else {
            memset(opt, 0, sizeof(opt));
            /* set print opt line */
            adjust_print_opt_line(opt, mod->opt_line, strlen(mod_hdr));
            /* set print hdr line */
            strcat(hdr_line, mod_hdr);
            strcat(opt_line, opt);
        }
        strcat(hdr_line, PRINT_SEC_SPLIT);
        strcat(opt_line, PRINT_SEC_SPLIT);
    }

    sprintf(header, "%s\n%s\n", opt_line, hdr_line);
    printf("%s", header);
}


void
printf_result(double result)
{
    if (conf.print_detail) {
        printf("%6.2f", result);
        printf("%s", PRINT_DATA_SPLIT);
        return;
    }

    if ((1000 - result) > 0.1) {
        printf("%6.2f", result);
    } else if ( (1000 - result / 1024) > 0.1) {
        printf("%5.1f%s", result / 1024, "K");
    } else if ((1000 - result / 1024 / 1024) > 0.1) {
        printf("%5.1f%s", result / 1024 / 1024, "M");
    } else if ((1000 - result / 1024 / 1024 / 1024) > 0.1) {
        printf("%5.1f%s", result / 1024 / 1024 / 1024, "G");
    } else if ((1000 - result / 1024 / 1024 / 1024 / 1024) > 0.1) {
        printf("%5.1f%s", result / 1024 / 1024 / 1024 / 1024, "T");
    }
    printf("%s", PRINT_DATA_SPLIT);
}


void
print_array_stat(const struct module *mod, const double *st_array)
{
    int    i;
    struct mod_info *info = mod->info;

    for (i = 0; i < mod->n_col; i++) {
        if (mod->spec) {
            /* print null */
            if (!st_array || !mod->st_flag || st_array[i] < 0) {
                /* print record */
                if (((DATA_SUMMARY == conf.print_mode) && (SPEC_BIT == info[i].summary_bit))
                        || ((DATA_DETAIL == conf.print_mode) && (SPEC_BIT == info[i].summary_bit)))
                {
                    printf("------%s", PRINT_DATA_SPLIT);
                }

            } else {
                /* print record */
                if (((DATA_SUMMARY == conf.print_mode) && (SPEC_BIT == info[i].summary_bit))
                        || ((DATA_DETAIL == conf.print_mode) && (SPEC_BIT == info[i].summary_bit)))
                {
                    printf_result(st_array[i]);
                }
            }

        } else {
            /* print null */
            if (!st_array || !mod->st_flag || st_array[i] < 0) {
                /* print record */
                if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[i].summary_bit))
                        || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[i].summary_bit)))
                {
                    printf("------%s", PRINT_DATA_SPLIT);
                }

            } else {
                /* print record */
                if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[i].summary_bit))
                        || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[i].summary_bit)))
                {
                    printf_result(st_array[i]);
                }
            }
        }
    }
}


/* print current time */
void
print_current_time()
{
    char    cur_time[LEN_32] = {0};
    time_t  timep;
    struct  tm *t;

    time(&timep);
    t = localtime(&timep);
    if (conf.running_mode == RUN_PRINT_LIVE) {
        strftime(cur_time, sizeof(cur_time), "%d/%m/%y-%T", t);

    } else {
        strftime(cur_time, sizeof(cur_time), "%d/%m/%y-%R", t);
    }
    printf("%s%s", cur_time, PRINT_SEC_SPLIT);
}


void
print_record()
{
    int        i, j;
    double    *st_array;
    struct     module *mod = NULL;

    /* print summary data */
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = mods[i];
        if (!mod->enable) {
            continue;
        }
        if (!mod->n_item) {
            print_array_stat(mod, NULL);
            printf("%s", PRINT_SEC_SPLIT);

        } else {
            for (j = 0; j < mod->n_item; j++) {
                if (*mod->print_item != 0 && (mod->p_item != j)) {
                    continue;
                }
                st_array = &mod->st_array[j * mod->n_col];
                print_array_stat(mod, st_array);
                printf("%s", PRINT_SEC_SPLIT);
            }
            if (mod->n_item > 1) {
                printf("%s", PRINT_SEC_SPLIT);
            }
        }
    }
    printf("\n");
}


/* running in print live mode */
void
running_print_live()
{
    int print_num = 1, re_p_hdr = 0, cost_time = 0, to_sleep = 0;

    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
    collect_record();

    /* print header */
    print_header();

    /* set struct module fields */
    init_module_fields();

    /* skip first record */
    if (collect_record_stat() == 0) {
        do_debug(LOG_INFO, "collect_record_stat warn\n");
    }
    gettimeofday(&tv_end, NULL);
    cost_time = (tv_end.tv_sec - tv_begin.tv_sec)*1000000 + (tv_end.tv_usec - tv_begin.tv_usec);
    to_sleep = conf.print_interval*1000000 - cost_time;
    if (to_sleep > 0) {
      usleep(to_sleep);
    }

    /* print live record */
    while (1) {
        gettimeofday(&tv_begin, NULL);
        collect_record();

        if (!((print_num) % DEFAULT_PRINT_NUM) || re_p_hdr) {
            /* get the header will print every DEFAULT_PRINT_NUM */
            print_header();
            re_p_hdr = 0;
            print_num = 1;
        }

        if (!collect_record_stat()) {
            re_p_hdr = 1;
            continue;
        }

        /* print current time */
        print_current_time();
        print_record();
        fflush(stdout);

        print_num++;

        /* sleep every interval */
        gettimeofday(&tv_end, NULL);
        cost_time = (tv_end.tv_sec - tv_begin.tv_sec)*1000000 + (tv_end.tv_usec - tv_begin.tv_usec);
        to_sleep = conf.print_interval*1000000 - cost_time;
        if (to_sleep > 0) {
          usleep(to_sleep);
        }
    }
}



/* find where start printting
 * number:the suffix number of record data (tsar.data.number)
 * return
 * 0 ok
 * 1 need find last tsar.data file
 * 2 find error, find time is later than the last line at tsar.data.x, should stop find any more
 * 3 find error, tsar haved stopped after find time, should stop find it
 * 4 find error, data not exist, tsar just lost some time data which contains find time
 * 5 log format error
 * 6 other error
 */
int
find_offset_from_start(FILE *fp, int number)
{
    long        fset, fend, file_len, off_start, off_end, offset, line_len;
    char       *p_sec_token;
    time_t      now, t_token, t_get;
    struct tm   stm;
    static char line[LEN_10M] = {0};

    /* get file len */
    if (fseek(fp, 0, SEEK_END) != 0 ) {
        do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
    }
    if ((fend = ftell(fp)) < 0) {
        do_debug(LOG_FATAL, "ftell error:%s", strerror(errno));
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
    }
    if ((fset = ftell(fp)) < 0) {
        do_debug(LOG_FATAL, "ftell error:%s", strerror(errno));
    }
    file_len = fend - fset;

    memset(&line, 0, LEN_10M);
    if (!fgets(line, LEN_10M, fp)) {
        do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
    }
    line_len = strlen(line);

    /* get time token */
    time(&now);
    if (conf.print_day > conf.print_max_day) {
        /*get specify date by --date/-d*/
        stm.tm_year = conf.print_day / 10000 - 1900;
        stm.tm_mon = conf.print_day % 10000 / 100 - 1;
        stm.tm_mday = conf.print_day % 100;
        stm.tm_hour = 0;
        stm.tm_min = 0;
        stm.tm_sec = 0;
        stm.tm_isdst = -1;
        t_token = mktime(&stm);
        conf.print_day = (now - t_token) / (24 * 60 * 60);
    }

    if (conf.print_day >= 0) {
        if (conf.print_day > conf.print_max_day) {
            conf.print_day = conf.print_max_day;
        }
        /* get day's beginning plus 8 hours.Set start and end time for print*/
        now = now - now % (24 * 60 * 60) - (8 * 60 * 60);
        t_token = now - conf.print_day * (24 * 60 * 60) - (60 * conf.print_nline_interval);
        conf.print_start_time = t_token;
        conf.print_end_time = t_token + 24 * 60 * 60 + (60 * conf.print_nline_interval);

    } else {
        /* set max days for print 6 months*/
        if (conf.print_ndays > conf.print_max_day) {
            conf.print_ndays = conf.print_max_day;
        }
        now = now - now % (60 * conf.print_nline_interval);
        if (conf.running_mode == RUN_WATCH) {
            if (conf.print_nminute > (conf.print_max_day * 24 * 60)) {
                conf.print_nminute = conf.print_max_day * 24 * 60;
            }
            t_token = now - (60 * conf.print_nminute) - (60 * conf.print_nline_interval);
        } else {
            t_token = now - conf.print_ndays * (24 * 60 * 60) - (60 * conf.print_nline_interval);
        }
        conf.print_start_time = t_token;
        conf.print_end_time = now + (60 * conf.print_nline_interval);
    }

    offset = off_start = 0;
    off_end = file_len;
    while (1) {
        offset = (off_start + off_end) / 2;
        memset(&line, 0, LEN_10M);
        if (fseek(fp, offset, SEEK_SET) != 0) {
            do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
        }
        if (!fgets(line, LEN_10M, fp) && errno != 0) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
        memset(&line, 0, LEN_10M);
        if (!fgets(line, LEN_10M, fp) && errno != 0) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
        if (0 != line[0] && offset > line_len) {
            p_sec_token = strpbrk(line, SECTION_SPLIT);
            if (p_sec_token) {
                *p_sec_token = '\0';
                t_get = atol(line);
                if (labs(t_get - t_token) <= 60) {
                    conf.print_file_number = number;
                    return 0;
                }

                /* Binary Search */
                if (t_get > t_token) {
                    off_end = offset;

                } else if (t_get < t_token) {
                    off_start = offset;
                }

            } else {
                /* fatal error, log format error happen. */
                return 5;
            }

        } else {
            if (off_end == file_len) {
                if (number > 0) {
                    conf.print_file_number = number - 1;
                    /* at the end of tsar.data.%d have some data lost during data rotate. stat from previous log file";*/
                    return 2;

                } else {
                    /* researching tsar.data to end and not find log data you need.";*/
                    return 3;
                }
            }
            if (off_start == 0) {
                conf.print_file_number = number;
                /* need to research tsar.data.number+1; */
                return 1;
            }
            /* here should not be arrived. */
            return 6;
        }

        if (offset == (off_start + off_end) / 2) {
            if (off_start != 0) {
                /* tsar has been down for a while, so the following time's stat we can provied only; */
                conf.print_file_number = number;
                return 4;
            }
            return 6;
        }
    }
}


/*
 * set and print record time
 */
long
set_record_time(const char *line)
{
    char        *token, s_time[LEN_32] = {0};
    static long  pre_time, c_time = 0;

    /* get record time */
    token = strpbrk(line, SECTION_SPLIT);
    memcpy(s_time, line, token - line);

    /* swap time */
    pre_time = c_time;
    c_time = atol(s_time);

    c_time = c_time - c_time % 60;
    pre_time = pre_time - pre_time % 60;
    /* if skip record when two lines haveing same minute */
    if (!(conf.print_interval = c_time - pre_time)) {
        return 0;

    } else {
        return c_time;
    }
}

/*
 * check time if corret for pirnt from tsar.data
 */
int
check_time(const char *line)
{
    char       *token, s_time[LEN_32] = {0};
    long        now_time = 0;
    static long pre_time;

    /* get record time */
    token = strpbrk(line, SECTION_SPLIT);
    if (token == NULL) {
        return 1;
    }
    if ((token - line) < 32) {
        memcpy(s_time, line, token - line);
    }
    now_time = atol(s_time);

    /* check if time is over print_end_time */
    if (now_time >= conf.print_end_time) {
        return 3;
    }
    /* if time is divide by conf.print_nline_interval*/
    now_time = now_time - now_time % 60;
    if (!((now_time - conf.print_start_time) % ( 60 * conf.print_nline_interval)) && now_time > pre_time) {
        /* check  now and last record time interval */
        if (pre_time && now_time - pre_time == ( 60 * conf.print_nline_interval)) {
            pre_time = now_time;
            return 0;
        }
        pre_time = now_time;
        return 1;

    } else {
        return 1;
    }
}

void
print_record_time(long c_time)
{
    char    s_time[LEN_32] = {0};
    struct  tm *t;

    t = localtime(&c_time);
    strftime(s_time, sizeof(s_time), "%d/%m/%y-%R", t);
    printf("%s%s", s_time, PRINT_SEC_SPLIT);
}


void
print_tail(int tail_type)
{
    int        i, j, k;
    double    *m_tail;
    struct     module *mod = NULL;

    switch (tail_type) {
        case TAIL_MAX:
            printf("MAX           %s", PRINT_SEC_SPLIT);
            break;
        case TAIL_MEAN:
            printf("MEAN          %s", PRINT_SEC_SPLIT);
            break;
        case TAIL_MIN:
            printf("MIN           %s", PRINT_SEC_SPLIT);
            break;
        default:
            return;
    }

    /* print summary data */
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = mods[i];
        if (!mod->enable) {
            continue;
        }
        switch (tail_type) {
            case TAIL_MAX:
                m_tail = mod->max_array;
                break;
            case TAIL_MEAN:
                m_tail = mod->mean_array;
                break;
            case TAIL_MIN:
                m_tail = mod->min_array;
                break;
            default:
                return;
        }

        k = 0;
        for (j = 0; j < mod->n_item; j++) {
            if (*mod->print_item != 0 && (mod->p_item != j)) {
                k += mod->n_col;
                continue;
            }
            int    i;
            struct mod_info *info = mod->info;
            for (i=0; i < mod->n_col; i++) {
                /* print record */
                if (mod->spec) {
                    if (((DATA_SUMMARY == conf.print_mode) && (SPEC_BIT == info[i].summary_bit))
                            || ((DATA_DETAIL == conf.print_mode) && (SPEC_BIT == info[i].summary_bit)))
                    {
                        printf_result(m_tail[k]);
                    }

                } else {
                    if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[i].summary_bit))
                            || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[i].summary_bit)))
                    {
                        printf_result(m_tail[k]);
                    }
                }
                k++;
            }
            printf("%s", PRINT_SEC_SPLIT);
        }
        if (mod->n_item != 1) {
            if (!m_tail) {
                print_array_stat(mod, NULL);
            }
            printf("%s", PRINT_SEC_SPLIT);
        }
    }
    printf("\n");
}


/*
 * init_running_print, if sucess then return fp, else return NULL
 */
FILE *
init_running_print()
{
    int           i=0, k=0;
    FILE         *fp, *fptmp;
    char          filename[LEN_128] = {0};
    static char   line[LEN_10M] = {0};

    /* will print tail*/
    conf.print_tail = 1;

    fp = fopen(conf.output_file_path, "r");
    if (!fp) {
        do_debug(LOG_FATAL, "unable to open the log file %s\n", conf.output_file_path);
    }
    /*log number to use for print*/
    conf.print_file_number = -1;
    /* find start offset will print from tsar.data */
    k=find_offset_from_start(fp, i);
    if (k == 1) {
        /*find all possible record*/
        for (i=1; ; i++) {
            memset(filename, 0, sizeof(filename));
            sprintf(filename, "%s.%d", conf.output_file_path, i);
            fptmp = fopen(filename, "r");
            if (!fptmp) {
                conf.print_file_number = i - 1;
                break;
            }

            k=find_offset_from_start(fptmp, i);
            if (k==0 || k==4) {
                if (fclose(fp) < 0) {
                    do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
                }
                fp=fptmp;
                break;
            }
            if (k== 2) {
                if (fseek(fp, 0, SEEK_SET) != 0) {
                    do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
                }
                if (fclose(fptmp) < 0) {
                    do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
                }
                break;
            }
            if (k == 1) {
                if (fclose(fp) < 0) {
                    do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
                }
                fp=fptmp;
                continue;
            }
            if (k == 5 || k == 6) {
                do_debug(LOG_FATAL, "log format error or find_offset_from_start have a bug. error code=%d\n", k);
            }
        }
    }

    if (k == 5 || k == 6) {
        do_debug(LOG_FATAL, "log format error or find_offset_from_start have a bug. error code=%d\n", k);
    }
    /* get record */
    if (!fgets(line, LEN_10M, fp)) {
        do_debug(LOG_FATAL, "can't get enough log info\n");
    }

    /* read one line to init module parameter */
    read_line_to_module_record(line);

    /* print header */
    print_header();

    /* set struct module fields */
    init_module_fields();

    set_record_time(line);
    return fp;
}


/*
 * print mode, print data from tsar.data
 */
void
running_print()
{
    int           print_num = 1, re_p_hdr = 0;
    char          filename[LEN_128] = {0};
    long          n_record = 0, s_time;
    FILE         *fp;
    static char   line[LEN_10M] = {0};

    /*find the position of the first record to be printed. (eg: middle of tsar.data.2)*/
    fp = init_running_print();

    /* skip first record */
    if (collect_record_stat() == 0) {
        do_debug(LOG_INFO, "collect_record_stat warn\n");
    }

    /*now ,print all printable records*/
    /*(eg: second half of tsar.data.2, then all of tsar.data.1, then tsar.data)*/
    while (1) {
        if (!fgets(line, LEN_10M, fp)) {
            if (conf.print_file_number <= 0) {
                break;

            } else {
                conf.print_file_number = conf.print_file_number - 1;
                memset(filename, 0, sizeof(filename));
                if (conf.print_file_number == 0) {
                    sprintf(filename, "%s", conf.output_file_path);

                } else {
                    sprintf(filename, "%s.%d", conf.output_file_path, conf.print_file_number);
                }
                if (fclose(fp) < 0) {
                    do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
                }
                fp = fopen(filename, "r");
                if (!fp) {
                    do_debug(LOG_FATAL, "unable to open the log file %s.\n", filename);
                }
                continue;
            }
        }

        int    k = check_time(line);
        if (k == 1) {
            continue;
        }
        if (k == 3) {
            break;
        }

        /* collect data then set mod->summary */
        read_line_to_module_record(line);

        if (!(print_num % DEFAULT_PRINT_NUM) || re_p_hdr) {
            /* get the header will print every DEFAULT_PRINT_NUM */
            print_header();
            re_p_hdr = 0;
            print_num = 1;
        }

        /* exclued the two record have same time */
        if (!(s_time = set_record_time(line))) {
            continue;
        }

        /* reprint header because of n_item's modifing */
        if (!collect_record_stat()) {
            re_p_hdr = 1;
            continue;
        }

        print_record_time(s_time);
        print_record();
        n_record++;
        print_num++;
        memset(line, 0, sizeof(line));
    }

    if (n_record) {
        printf("\n");
        print_tail(TAIL_MAX);
        print_tail(TAIL_MEAN);
        print_tail(TAIL_MIN);
    }

    if (fclose(fp) < 0) {
        do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
    }
    fp = NULL;
}

char *
trim(char* src, int max_len)
{
    int    cur_len = 0;
    char  *index=src;

    while (*index == ' ' && cur_len<max_len) {
        index++;
        cur_len++;
    }
    return index;
}

int
seek_tail_lines(FILE *fp, int n, int len[])
{
    int total_num = 0;

    /* find two \n from end*/
    if (fseek(fp, -1, SEEK_END) != 0) {
        do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
    }
    while (1) {
        if (fgetc(fp) == '\n') {
            ++total_num;
            len[n - total_num] = 0;
        } else {
            ++len[n - total_num];
        }
        if (total_num > n) {
            break;
        }
        if (fseek(fp, -2, SEEK_CUR) != 0) {
            /* goto file head */
            if (fseek(fp, 0, SEEK_SET) != 0) {
                do_debug(LOG_FATAL, "fseek error:%s", strerror(errno));
            }
            break;
        }
    }

    return total_num;
}

void
running_check(int check_type)
{
    int               total_num, len[2] = {0}, i, j, k;
    FILE             *fp;
    char              filename[LEN_128] = {0};
    char              tmp[10][LEN_4096];
    char              host_name[LEN_64] = {0};
    struct            module *mod = NULL;
    struct            stat statbuf;
    time_t            nowtime, ts[2] = {0};
    double           *st_array;
    char             *line[2];
    static char       check[LEN_4096 * 11] = {0};

    /* get hostname */
    if (0 != gethostname(host_name, sizeof(host_name))) {
        do_debug(LOG_FATAL, "tsar -check: gethostname err, errno=%d", errno);
    }
    i = 0;
    while (host_name[i]) {
        if (!isprint(host_name[i++])) {
            host_name[i-1] = '\0';
            break;
        }
    }
    sprintf(filename, "%s", conf.output_file_path);
    fp = fopen(filename, "r");
    if (!fp) {
        do_debug(LOG_FATAL, "unable to open the log file %s.\n", filename);
    }
    /* check file update time */
    stat(filename, &statbuf);
    time(&nowtime);
    if (nowtime - statbuf.st_mtime > 300) {
        do_debug(LOG_FATAL, "/var/log/tsar.data is far away from now, now time is %d, last time is %d", nowtime, statbuf.st_mtime);
    }
    /*FIX ME*/
    total_num = seek_tail_lines(fp, 2, len);
    if (total_num == 0) {
        if (fclose(fp) < 0) {
            do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
        }
        sprintf(filename, "%s.1", conf.output_file_path);
        fp = fopen(filename, "r");
        if (!fp) {
            do_debug(LOG_FATAL, "unable to open the log file %s.\n", filename);
        }
        /* count tsar.data.1 lines */
        total_num = seek_tail_lines(fp, 2, len);
        if (total_num < 2) {
            do_debug(LOG_FATAL, "not enough lines at log file %s.\n", filename);
        }

        line[0] = malloc(len[0] + 2);
        if (!fgets(line[0], len[0] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
        line[1] = malloc(len[1] + 2);
        if (!fgets(line[1], len[1] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }

    } else if (total_num == 1) {
        line[1] = malloc(len[1] + 2);
        if (!fgets(line[1], len[1] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
        if (fclose(fp) < 0) {
            do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
        }
        sprintf(filename, "%s.1", conf.output_file_path);
        fp = fopen(filename, "r");
        if (!fp) {
            do_debug(LOG_FATAL, "unable to open the log file %s\n", filename);
        }
        /* go to the start of the last line at tsar.data.1 */
        total_num = seek_tail_lines(fp, 1, len);

        if (total_num < 1) {
            do_debug(LOG_FATAL, "not enough lines at log file %s\n", filename);
        }
        line[0] = malloc(len[0] + 2);
        if (!fgets(line[0], len[0] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }

    } else {
        line[0] = malloc(len[0] + 2);
        if (!fgets(line[0], len[0] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
        line[1] = malloc(len[1] + 2);
        if (!fgets(line[1], len[1] + 2, fp)) {
            do_debug(LOG_FATAL, "fgets error:%s", strerror(errno));
        }
    }

    /*as fp is not used after here,close it */
    if (fclose(fp) < 0) {
        do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
    }
    fp = NULL;

    /* set struct module fields */
    init_module_fields();

    /* read one line to init module parameter */
    ts[0] = read_line_to_module_record(line[0]);
    free(line[0]);
    collect_record_stat();

    ts[1] = read_line_to_module_record(line[1]);
    free(line[1]);
    if (ts[0] && ts[1]) {
        conf.print_interval = ts[1] - ts[0];
        if (conf.print_interval == 0) {
            do_debug(LOG_FATAL, "running tsar -c too frequently");
            return;
        }
    }
    collect_record_stat();

    /*display check detail*/
    /* ---------------------------RUN_CHECK_NEW--------------------------------------- */
    if (check_type == RUN_CHECK_NEW) {
        printf("%s\ttsar\t", host_name);
        for (i = 0; i < statis.total_mod_num; i++) {
            mod = mods[i];
            if (!mod->enable) {
                continue;
            }
            struct mod_info *info = mod->info;
            /* get mod name */
            char *mod_name = strstr(mod->opt_line, "--");
            if (mod_name) {
                mod_name += 2;
            }

            char opt[LEN_128] = {0};
            char *n_record = strdup(mod->record);
            char *token = strtok(n_record, ITEM_SPLIT);
            char *s_token;

            for (j = 0; j < mod->n_item; j++) {
                memset(opt, 0, sizeof(opt));
                if (token) {
                    s_token = strpbrk(token, ITEM_SPSTART);
                    if (s_token) {
                        strncat(opt, token, s_token - token);
                        strcat(opt, ":");
                    }
                }
                st_array = &mod->st_array[j * mod->n_col];
                for (k=0; k < mod->n_col; k++) {
                    if (mod->spec) {
                        if (!st_array || !mod->st_flag) {
                            if (((DATA_SUMMARY == conf.print_mode) && (SPEC_BIT == info[k].summary_bit))
                                    || ((DATA_DETAIL == conf.print_mode) && (SPEC_BIT == info[k].summary_bit)))
                            {
                                printf("%s:%s%s=-%s", mod_name, opt, trim(info[k].hdr, LEN_128), " ");
                            }

                        } else {
                            if (((DATA_SUMMARY == conf.print_mode) && (SPEC_BIT == info[k].summary_bit))
                                    || ((DATA_DETAIL == conf.print_mode) && (SPEC_BIT == info[k].summary_bit)))
                            {
                                printf("%s:%s%s=", mod_name, opt, trim(info[k].hdr, LEN_128));
                                printf("%0.1f ", st_array[k]);
                            }
                        }

                    } else {
                        if (!st_array || !mod->st_flag) {
                            if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[k].summary_bit))
                                    || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[k].summary_bit)))
                            {
                                printf("%s:%s%s=-%s", mod_name, opt, trim(info[k].hdr, LEN_128), " ");
                            }

                        } else {
                            if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[k].summary_bit))
                                    || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[k].summary_bit)))
                            {
                                printf("%s:%s%s=", mod_name, opt, trim(info[k].hdr, LEN_128));
                                printf("%0.1f ", st_array[k]);
                            }
                        }

                    }
                }
                if (token) {
                    token = strtok(NULL, ITEM_SPLIT);
                }
            }
            if (n_record) {
                free(n_record);
                n_record = NULL;
            }
        }
        printf("\n");
        return;
    }
#ifdef OLDTSAR
    /*tsar -check output similar as:
      v014119.cm3   tsar   apache/qps=5.35 apache/rt=165.89 apache/busy=2 apache/idle=148 cpu=3.58 mem=74.93% load1=0.22 load5=0.27 load15=0.20 xvda=0.15 ifin=131.82 ifout=108.86 TCPretr=0.12 df/=4.04% df/home=10.00% df/opt=71.22% df/tmp=2.07% df/usr=21.27% df/var=5.19%
     */
    /* ------------------------------RUN_CHECK------------------------------------------- */
    if (check_type == RUN_CHECK) {
        //memset(tmp, 0, 10 * LEN_4096);
        sprintf(check, "%s\ttsar\t", host_name);
        for (i = 0; i < statis.total_mod_num; i++) {
            mod = mods[i];
            if (!mod->enable){
                continue;
            }
            if (!strcmp(mod->name, "mod_apache")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[0], " apache/qps=- apache/rt=- apache/busy=- apache/idle=-");

                    } else {
                        sprintf(tmp[0], " apache/qps=%0.2f apache/rt=%0.2f apache/busy=%0.0f apache/idle=%0.0f", st_array[0], st_array[1], st_array[3], st_array[4]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_cpu")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[1], " cpu=-");

                    } else {
                        sprintf(tmp[1], " cpu=%0.2f", st_array[5]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_mem")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[2], " mem=-");

                    } else {
                        sprintf(tmp[2], " mem=%0.2f%%", st_array[5]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_load")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[3], " load1=- load5=- load15=-");

                    } else {
                        sprintf(tmp[3], " load1=%0.2f load5=%0.2f load15=%0.2f", st_array[0], st_array[1], st_array[2]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_io")) {
                char    opt[LEN_128] = {0};
                char    item[LEN_128] = {0};
                char   *n_record = strdup(mod->record);
                char   *token = strtok(n_record, ITEM_SPLIT);
                char   *s_token;
                for (j = 0; j < mod->n_item && token != NULL; j++) {
                    s_token = strpbrk(token, ITEM_SPSTART);
                    if (s_token) {
                        memset(opt, 0, sizeof(opt));
                        strncat(opt, token, s_token - token);
                        st_array = &mod->st_array[j * mod->n_col];
                        if (!st_array || !mod->st_flag) {
                            sprintf(item, " %s=-", opt);

                        } else {
                            sprintf(item, " %s=%0.2f", opt, st_array[10]);
                        }
                        strcat(tmp[4], item);
                    }
                    token = strtok(NULL, ITEM_SPLIT);
                }
                if (n_record) {
                    free(n_record);
                    n_record = NULL;
                }
            }
            if (!strcmp(mod->name, "mod_traffic")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[5], " ifin=- ifout=-");

                    } else {
                        sprintf(tmp[5], " ifin=%0.2f ifout=%0.2f", st_array[0] / 1000, st_array[1] / 1000);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_tcp")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[6], " TCPretr=-");

                    } else {
                        sprintf(tmp[6], " TCPretr=%0.2f", st_array[7]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_partition")) {
                char    opt[LEN_128] = {0};
                char    item[LEN_128] = {0};
                char   *n_record = strdup(mod->record);
                char   *token = strtok(n_record, ITEM_SPLIT);
                char   *s_token;
                for (j = 0; j < mod->n_item && token != NULL; j++) {
                    s_token = strpbrk(token, ITEM_SPSTART);
                    if (s_token) {
                        memset(opt, 0, sizeof(opt));
                        strncat(opt, token, s_token - token);
                        st_array = &mod->st_array[j * mod->n_col];
                        if (!st_array || !mod->st_flag) {
                            sprintf(item, " df%s=-", opt);

                        } else {
                            sprintf(item, " df%s=%0.2f%%", opt, st_array[3]);
                        }
                        strcat(tmp[7], item);
                    }
                    token = strtok(NULL, ITEM_SPLIT);
                }
                if (n_record) {
                    free(n_record);
                    n_record = NULL;
                }
            }
            if (!strcmp(mod->name, "mod_nginx")){
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[8], " nginx/qps=- nginx/rt=-");

                    } else {
                        sprintf(tmp[8], " nginx/qps=%0.2f nginx/rt=%0.2f", st_array[7], st_array[8]);
                    }
                }
            }
            if (!strcmp(mod->name, "mod_swap")) {
                for (j = 0; j < mod->n_item; j++) {
                    st_array = &mod->st_array[j * mod->n_col];
                    if (!st_array || !mod->st_flag) {
                        sprintf(tmp[9], " swap/total=- swap/util=-");

                    } else {
                        sprintf(tmp[9], " swap/total=%0.2f swap/util=%0.2f%%", st_array[2] / 1024 / 1024, st_array[3]);
                    }
                }
            }
        }
        for (j = 0; j < 10; j++) {
            strcat(check, tmp[j]);
        }
        printf("%s\n", check);
    }
#endif
}
/*end*/

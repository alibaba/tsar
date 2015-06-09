
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


int
is_digit(const char *str)
{
    if (*str == '-') {
        str++;
    }
    /*dont handle minus value in tsar.data */
    while (*str) {
        if (!isdigit(*str++)) {
            return 0;
        }
    }
    return 1;
}


/*
 * convert record to array
 */
int
convert_record_to_array(U_64 *array, int l_array, const char *record)
{
    int     i = 0;
    char   *token;
    char    n_str[LEN_1M] = {0};

    if (!record || !strlen(record)) {
        return 0;
    }
    memcpy(n_str, record, strlen(record));

    token = strtok(n_str, DATA_SPLIT);
    while (token) {
        if (!is_digit(token)) {
            return 0;
        }
        if (i < l_array) {
            *(array + i) = strtoull(token, NULL, 10);
        }
        token = strtok(NULL, DATA_SPLIT);
        i++;
    }
    /* support add col*/
    if (i > l_array) {
        return 0;
    }
    return i;
}


int
merge_one_string(U_64 *array, int l_array, char *string, struct module *mod, int n_item)
{
    int    i, len;
    U_64   array_2[MAX_COL_NUM] = {0};
    struct mod_info *info = mod->info;

    if (!(len = convert_record_to_array(array_2, l_array, string))) {
        return 0;
    }

    for (i=0; i < len; i++) {
        switch (info[i].merge_mode) {
            case MERGE_SUM:
                array[i] += array_2[i];
                break;
            case MERGE_AVG:
                array[i] = (array[i] * (n_item - 1) + array_2[i]) / n_item;
                break;
            default:
                ;
        }
    }
    return 1;
}


int
strtok_next_item(char item[], char *record, int *start)
{
    char *s_token, *e_token, *n_record;

    if (!record || !strlen(record) || strlen(record) <= *start) {
        return 0;
    }

    n_record = record + *start;
    e_token = strstr(n_record, ITEM_SPLIT);
    if (!e_token) {
        return 0;
    }
    s_token = strstr(n_record, ITEM_SPSTART);
    if (!s_token) {
        return 0;
    }
    if (e_token < s_token) {
        return 0;
    }

    memcpy(item, s_token + sizeof(ITEM_SPSTART) - 1, e_token - s_token - 1);
    *start = e_token - record + sizeof(ITEM_SPLIT);
    return 1;
}


int
merge_mult_item_to_array(U_64 *array, struct module *mod)
{
    int    pos = 0;
    int    n_item = 1;
    char   item[LEN_1M] = {0};

    memset(array, 0, sizeof(U_64) * mod->n_col);
    while (strtok_next_item(item, mod->record, &pos)) {
        if (!merge_one_string(array, mod->n_col, item, mod, n_item)) {
            return 0;
        }
        n_item++;
        memset(&item, 0, sizeof(item));
    }
    return 1;
}


int
get_strtok_num(const char *str, const char *split)
{
    int    num = 0;
    char  *token, n_str[LEN_1M] = {0};

    if (!str || !strlen(str)) {
        return 0;
    }

    memcpy(n_str, str, strlen(str));
    /* set print opt line */
    token = strtok(n_str, split);
    while (token) {
        num++;
        token = strtok(NULL, split);
    }

    return num;
}


/*
 * get__mod_hdr;  hdr format:HDR_SPLIT"hdr1"HDR_SLIT"hdr2"
 */
void
get_mod_hdr(char hdr[], const struct module *mod)
{
    int    i, pos = 0;
    struct mod_info *info = mod->info;

    for (i = 0; i < mod->n_col; i++) {
        if (mod->spec) {
            if (SPEC_BIT == info[i].summary_bit) {
                if (strlen(info[i].hdr) > 6) {
                    info[i].hdr[6] = '\0';
                }
                pos += sprintf(hdr + pos, "%s%s", info[i].hdr, PRINT_DATA_SPLIT);
            }

        } else {
            if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[i].summary_bit))
                || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[i].summary_bit)))
            {
                if (strlen(info[i].hdr) > 6) {
                    info[i].hdr[6] = '\0';
                }
                pos += sprintf(hdr + pos, "%s%s", info[i].hdr, PRINT_DATA_SPLIT);
            }
        }
    }
}


/*
   get data from tsar.data
 */
int
get_st_array_from_file(int have_collect)
{
    int         i, ret = 0;
    char        detail[LEN_1M] = {0};
    char        pre_time[32] = {0};
    char       *s_token;
    FILE       *fp;
    struct      module *mod;
    static char pre_line[LEN_10M] = {0};
    static char line[LEN_10M] = {0};

    if (!have_collect) {
        collect_record(0);
    }

    /* update module parameter */
    conf.print_merge = MERGE_ITEM;

    sprintf(line, "%ld", statis.cur_time);
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (mod->enable && strlen(mod->record)) {
            memset(&detail, 0, sizeof(detail));
            /* save collect data to output_file */
            sprintf(detail, "%s%s%s%s", SECTION_SPLIT, mod->opt_line, STRING_SPLIT, mod->record);
            strcat(line, detail);
        }
    }

    if (strlen(line)) {
        strcat(line, "\n");
    }

    /* if fopen PRE_RECORD_FILE sucess then store data to pre_record */
    if ((fp = fopen(PRE_RECORD_FILE, "r"))) {
        if (!fgets(pre_line, LEN_10M, fp)) {
            if (fclose(fp) < 0) {
                do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
            }
            ret = -1;
            goto out;
        } else {
             if (fclose(fp) < 0) {
                do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
            }
        }
    } else {
        ret = -1;
        goto out;
    }

    /* set print_interval */
    s_token = strstr(pre_line, SECTION_SPLIT);
    if (!s_token) {
        ret = -1;
        goto out;
    }
    memcpy(pre_time, pre_line, s_token - pre_line);
    if (!(conf.print_interval = statis.cur_time - atol(pre_time))) {
        ret = -1;
        goto out;
    }

    /* read pre_line to mod->record and store to pre_array */
    read_line_to_module_record(pre_line);
    init_module_fields();
    collect_record_stat();

    /* read cur_line and stats operation */
    read_line_to_module_record(line);
    collect_record_stat();
    ret = 0;

out:
    /* store current record to PRE_RECORD_FILE */
    if ((fp = fopen(PRE_RECORD_FILE, "w"))) {
        strcat(line, "\n");
        if (fputs(line, fp) < 0) {
            do_debug(LOG_ERR, "fputs error:%s", strerror(errno));
        }
        if (fclose(fp) < 0) {
            do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
        }
        chmod(PRE_RECORD_FILE, 0666);
    }

    return ret;
}

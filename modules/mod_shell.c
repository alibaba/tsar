
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


/*

read shell data from file
file format is as bellow:
---------------------
value1 value2 value3
1 2 3
---------------------
line 1 is every item name
line 2 is data for this item, please update this value by your shell
*/

char *shell_usage = "    --shell             My shell data collect";

/* Structure for tsar */
static struct mod_info shell_info[MAX_COL_NUM];

static void
read_shell_stats(struct module *mod, const char *parameter)
{
    /* parameter actually equals to mod->parameter */
    int     i = 0, pos = 0;
    FILE   *fp;
    char    line[LEN_4096];
    char    buf[LEN_4096];
    memset(buf, 0, sizeof(buf));
    if ((fp = fopen(parameter, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_4096, fp) != NULL) {
        unsigned long long value;
        if (i == 1) {
            char *p;
            char *delim = " \r\n";
            p = strtok(line, delim);
            value = strtoll(p, NULL, 10);
            pos = sprintf(buf, "%llu,", value);
            while ((p = strtok(NULL, delim))) {
                value = strtol(p, NULL, 10);
                pos += sprintf(buf + pos, "%llu,", value);
            }
        }
        if (i++ > 0) {
            break;
        }
    }
    fclose(fp);
    buf[pos-1] = '\0';
    buf[pos] = '\0';
    /* send data to tsar you can get it by pre_array&cur_array at set_shell_record */
    set_mod_record(mod, buf);
    return;
}

static void
set_shell_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    /* set st record */
    for (i = 0; i < mod->n_col; i++) {
        st_array[i] = cur_array[i];
    }
}

/* register mod to tsar */
void
mod_register(struct module *mod)
{
    /* parameter actually equals to mod->parameter */
    int     count = 0; 
    FILE   *fp;
    char    line[LEN_4096];
    if ((fp = fopen(mod->parameter, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_4096, fp) != NULL) {
        char *p;
        char *delim = " \r\n";
        p = strtok(line, delim);
        shell_info[count].summary_bit = DETAIL_BIT;
        shell_info[count].merge_mode = MERGE_NULL;
        shell_info[count].stats_opt = STATS_NULL;
        sprintf(shell_info[count].hdr, "%s", p);
        count++;
        while ((p = strtok(NULL, delim))) {
            shell_info[count].summary_bit = DETAIL_BIT;
            shell_info[count].merge_mode = MERGE_NULL;
            shell_info[count].stats_opt = STATS_NULL;
            sprintf(shell_info[count].hdr, "%s", p);
            count++;
            if (count == MAX_COL_NUM) {
                break;
            }
        }
        break;
    }
    fclose(fp);

    register_mod_fields(mod, "--shell", shell_usage, shell_info, count, read_shell_stats, set_shell_record);
}

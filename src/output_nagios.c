
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


#include <fcntl.h>
#include "tsar.h"


#define PRE_RECORD_FILE     "/tmp/.tsar.tmp"


void
output_nagios()
{
    int         i = 0, j = 0, k = 0, l = 0, result = 0, now_time;
    char        s_time[LEN_64] = {0};
    char        host_name[LEN_64] = {0};
    struct      module *mod;
    static char output[LEN_10M] = {0};
    static char output_err[LEN_10M] = {0};

    /* if cycle time ok*/
    now_time = statis.cur_time - statis.cur_time%60;
    if (conf.cycle_time == 0 || now_time%conf.cycle_time != 0) {
        return;
    }

    /* get hostname */
    if (0 != gethostname(host_name, sizeof(host_name))) {
        do_debug(LOG_FATAL, "send to nagios: gethostname err, errno=%d \n", errno);
    }
    while (host_name[i]) {
        if (!isprint(host_name[i++])) {
            host_name[i-1] = '\0';
            break;
        }
    }

    /* update module parameter */
    conf.print_merge = MERGE_NOT;

    /* get st_array */
    if (get_st_array_from_file(0)) {
        return;
    }

    /* only output from output_nagios_mod */
    reload_modules(conf.output_nagios_mod);

    sprintf(s_time, "%ld", time(NULL));

    /* print summary data */
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;

        } else {
            if (!mod->st_flag) {
                printf("name %s\n", mod->name);
                printf("do nothing\n");

            } else {
                char                opt[LEN_32];
                char                check[LEN_64];
                char               *n_record = strdup(mod->record);
                char               *token = strtok(n_record, ITEM_SPLIT);
                char               *s_token;
                double             *st_array;
                struct mod_info    *info = mod->info;

                j = 0;
                /* get mod_name.(item_name).col_name value */
                while (token) {
                    memset(check, 0, sizeof(check));
                    strcat(check, mod->name + 4);
                    strcat(check, ".");
                    s_token = strstr(token, ITEM_SPSTART);
                    /* multi item */
                    if (s_token){
                        memset(opt, 0, sizeof(opt));
                        strncat(opt, token, s_token - token);
                        strcat(check, opt);
                        strcat(check, ".");
                    }
                    /* get value */
                    st_array = &mod->st_array[j * mod->n_col];
                    token = strtok(NULL, ITEM_SPLIT);
                    j++;
                    for (k = 0; k < mod->n_col; k++) {
                        char    *p;
                        char    check_item[LEN_64];

                        memset(check_item, 0, LEN_64);
                        memcpy(check_item, check, LEN_64);
                        p = info[k].hdr;
                        while (*p == ' ') {
                            p++;
                        }
                        strcat(check_item, p);
                        for (l = 0; l < conf.mod_num; l++){
                            /* cmp tsar item with naigos item*/
                            if (!strcmp(conf.check_name[l], check_item)) {
                                char    value[LEN_32];
                                memset(value, 0, sizeof(value));
                                sprintf(value, "%0.2f", st_array[k]);
                                strcat(output, check_item);
                                strcat(output, "=");
                                strcat(output, value);
                                strcat(output, " ");
                                if (conf.cmin[l] != 0 && st_array[k] >= conf.cmin[l]) {
                                    if (conf.cmax[l] == 0 || (conf.cmax[l] != 0 && st_array[k] <= conf.cmax[l])) {
                                        result = 2;
                                        strcat(output_err, check_item);
                                        strcat(output_err, "=");
                                        strcat(output_err, value);
                                        strcat(output_err, " ");
                                        continue;
                                    }
                                }
                                if (conf.wmin[l] != 0 && st_array[k] >= conf.wmin[l]) {
                                    if (conf.wmax[l] == 0 || (conf.wmax[l] != 0 && st_array[k] <= conf.wmax[l]) ) {
                                        if (result != 2) {
                                            result = 1;
                                        }
                                        strcat(output_err, check_item);
                                        strcat(output_err, "=");
                                        strcat(output_err, value);
                                        strcat(output_err, " ");
                                    }
                                }
                            }
                        }
                    }
                }
                free(n_record);
            }
        }
    }
    if (!strcmp(output_err, "")) {
        strcat(output_err, "OK");
    }
    /* send to nagios server*/
    char    nagios_cmd[LEN_1024];
    sprintf(nagios_cmd, "echo \"%s;tsar;%d;%s|%s\"|%s -H %s -p %d -to 10 -d \";\" -c %s", host_name, result, output_err, output, conf.send_nsca_cmd, conf.server_addr, conf.server_port, conf.send_nsca_conf);
    do_debug(LOG_DEBUG, "send to naigos:%s\n", nagios_cmd);
    if (system(nagios_cmd) != 0) {
        do_debug(LOG_WARN, "nsca run error:%s\n", nagios_cmd);
    }
    printf("%s\n", nagios_cmd);
}

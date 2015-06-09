
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


void
register_mod_fields(struct module *mod, const char *opt, const char *usage,
    struct mod_info *info, int n_col, void *data_collect, void *set_st_record)
{
    sprintf(mod->opt_line, "%s", opt);
    sprintf(mod->usage, "%s", usage);
    mod->info = info;
    mod->n_col = n_col;
    mod->data_collect = data_collect;
    mod->set_st_record = set_st_record;
}


void
set_mod_record(struct module *mod, const char *record)
{
    if (record) {
        sprintf(mod->record, "%s", record);
    }
}


/*
 * load module from dir
 */
void
load_modules()
{
    int     i;
    char    buff[LEN_128] = {0};
    char    mod_path[LEN_128] = {0};
    struct  module *mod = NULL;
    int    (*mod_register) (struct module *);

    /* get the full path of modules */
    sprintf(buff, "/usr/local/tsar/modules");

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->lib) {
            memset(mod_path, '\0', LEN_128);
            snprintf(mod_path, LEN_128, "%s/%s.so", buff, mod->name);
            if (!(mod->lib = dlopen(mod_path, RTLD_NOW|RTLD_GLOBAL))) {
                do_debug(LOG_ERR, "load_modules: dlopen module %s err %s\n", mod->name, dlerror());

            } else {
                mod_register = dlsym(mod->lib, "mod_register");
                if (dlerror()) {
                    do_debug(LOG_ERR, "load_modules: dlsym module %s err %s\n", mod->name, dlerror());
                    break;

                } else {
                    mod_register(mod);
                    mod->enable = 1;
                    mod->spec = 0;
                    do_debug(LOG_INFO, "load_modules: load new module '%s' to mods\n", mod_path);
                }
            }
        }
    }
}


/*
 * reload modules by mods, if not find in mods, then set module disable
 * return 1 if mod load ok
 * return 0 else
 */
int
reload_modules(const char *s_mod)
{
    int    i;
    int    reload = 0;
    char   buf[LEN_512], name[LEN_64], *token, *param;

    if (!s_mod || !strlen(s_mod)) {
        return reload;
    }

    strncpy(buf, s_mod, strlen(s_mod) + 1);

    for (i = 0; i < statis.total_mod_num; i++)
        mods[i].enable = 0;

    token = strtok(buf, DATA_SPLIT);
    while (token != NULL) {
        strncpy(name, token, strlen(token) + 1);

        /* extract the parameter specified in the command line */
        param = strchr(name, PARAM_SPLIT);
        if (param != NULL) {
            *param = '\0';
            ++param;
        }

        for (i = 0; i < statis.total_mod_num; i++) {
            if (strcmp(name, mods[i].name) == 0
                    || strcmp(name, mods[i].opt_line) == 0) {
                reload = 1;
                mods[i].enable = 1;

                if (param != NULL) {
                    strncpy(mods[i].parameter, param, strlen(param) + 1);
                }

                break;
            }
        }

        token = strtok(NULL, DATA_SPLIT);
    }

    return reload;
}

#ifdef OLDTSAR
/*
 * reload check modules by mods, if not find in mods, then set module disable
 */
void
reload_check_modules()
{
    int    i;
    struct module *mod;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!strcmp(mod->name, "mod_apache")
             || !strcmp(mod->name, "mod_cpu")
             || !strcmp(mod->name, "mod_mem")
             || !strcmp(mod->name, "mod_load")
             || !strcmp(mod->name, "mod_partition")
             || !strcmp(mod->name, "mod_io")
             || !strcmp(mod->name, "mod_tcp")
             || !strcmp(mod->name, "mod_traffic")
             || !strcmp(mod->name, "mod_nginx")
             || !strcmp(mod->name, "mod_swap"))
        {
            mod->enable = 1;

        } else {
            mod->enable = 0;
        }
    }
}
/*end*/
#endif

/*
 * 1. alloc or realloc store array
 * 2. set mod->n_item
 */
void
init_module_fields()
{
    int    i;
    struct module *mod = NULL;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;
        }

        if (MERGE_ITEM == conf.print_merge) {
            mod->n_item = 1;

        } else {
            /* get mod->n_item first, and mod->n_item will be reseted in reading next line */
            mod->n_item = get_strtok_num(mod->record, ITEM_SPLIT);
        }

        if (mod->n_item) {
            mod->pre_array = (U_64 *)calloc(mod->n_item * mod->n_col, sizeof(U_64));
            mod->cur_array = (U_64 *)calloc(mod->n_item * mod->n_col, sizeof(U_64));
            mod->st_array = (double *)calloc(mod->n_item * mod->n_col, sizeof(double));
            if (conf.print_tail) {
                mod->max_array = (double *)calloc(mod->n_item * mod->n_col, sizeof(double));
                mod->mean_array = (double *)calloc(mod->n_item * mod->n_col, sizeof(double));
                mod->min_array = (double *)calloc(mod->n_item * mod->n_col, sizeof(double));
            }
        }
    }
}


/*
 * 1. realloc store array when mod->n_item is modify
 */
void
realloc_module_array(struct module *mod, int n_n_item)
{
    if (n_n_item > mod->n_item) {
        if (mod->pre_array) {
            mod->pre_array = (U_64 *)realloc(mod->pre_array, n_n_item * mod->n_col * sizeof(U_64));
            mod->cur_array = (U_64 *)realloc(mod->cur_array, n_n_item * mod->n_col * sizeof(U_64));
            mod->st_array = (double *)realloc(mod->st_array, n_n_item * mod->n_col * sizeof(double));
            if (conf.print_tail) {
                mod->max_array = (double *)realloc(mod->max_array, n_n_item * mod->n_col *sizeof(double));
                mod->mean_array =(double *)realloc(mod->mean_array, n_n_item * mod->n_col *sizeof(double));
                mod->min_array = (double *)realloc(mod->min_array, n_n_item * mod->n_col *sizeof(double));
            }

        } else {
            mod->pre_array = (U_64 *)calloc(n_n_item * mod->n_col, sizeof(U_64));
            mod->cur_array = (U_64 *)calloc(n_n_item * mod->n_col, sizeof(U_64));
            mod->st_array = (double *)calloc(n_n_item * mod->n_col, sizeof(double));
            if (conf.print_tail) {
                mod->max_array = (double *)calloc(n_n_item * mod->n_col, sizeof(double));
                mod->mean_array =(double *)calloc(n_n_item * mod->n_col, sizeof(double));
                mod->min_array = (double *)calloc(n_n_item * mod->n_col, sizeof(double));
            }
        }
    }
}

/*
 * set st result in st_array
 */
void
set_st_record(struct module *mod)
{
    int    i, j, k = 0;
    struct mod_info *info = mod->info;

    mod->st_flag = 1;

    for (i = 0; i < mod->n_item; i++) {
        /* custom statis compute */
        if (mod->set_st_record) {
            mod->set_st_record(mod, &mod->st_array[i * mod->n_col],
                    &mod->pre_array[i * mod->n_col],
                    &mod->cur_array[i * mod->n_col],
                    conf.print_interval);
        }

        for (j=0; j < mod->n_col; j++) {
            if (!mod->set_st_record) {
                switch (info[j].stats_opt) {
                    case STATS_SUB:
                        if (mod->cur_array[k] < mod->pre_array[k]) {
                            mod->pre_array[k] = mod->cur_array[k];
                            mod->st_flag = 0;

                        } else {
                            mod->st_array[k] = mod->cur_array[k] - mod->pre_array[k];
                        }
                        break;
                    case STATS_SUB_INTER:
                        if (mod->cur_array[k] < mod->pre_array[k]) {
                            mod->pre_array[k] = mod->cur_array[k];
                            mod->st_flag = 0;

                        } else {
                            mod->st_array[k] = (mod->cur_array[k] -mod->pre_array[k]) / conf.print_interval;
                        }
                        break;
                    default:
                        mod->st_array[k] = mod->cur_array[k];
                }
                mod->st_array[k] *= 1.0;
            }

            if (conf.print_tail) {
                if (0 == mod->n_record) {
                    mod->max_array[k] = mod->mean_array[k] = mod->min_array[k] = mod->st_array[k] * 1.0;

                } else {
                    if (mod->st_array[k] - mod->max_array[k] > 0.1) {
                        mod->max_array[k] = mod->st_array[k];
                    }
                    if (mod->min_array[k] - mod->st_array[k] > 0.1 && mod->st_array[k] >= 0) {
                        mod->min_array[k] = mod->st_array[k];
                    }
                    if (mod->st_array[k] >= 0) {
                        mod->mean_array[k] = ((mod->n_record - 1) *mod->mean_array[k] + mod->st_array[k]) / mod->n_record;
                    }
                }
            }
            k++;
        }
    }

    mod->n_record++;
}


/*
 * if diable = 1, then will disable module when record is null
 */
void
collect_record()
{
    int    i;
    struct module *mod = NULL;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;
        }

        memset(mod->record, 0, sizeof(mod->record));
        if (mod->data_collect) {
            mod->data_collect(mod, mod->parameter);
        }
    }
}


/*
 * computer mod->st_array and swap cur_info to pre_info
 * return:  1 -> ok
 *      0 -> some mod->n_item have modify will reprint header
 */
int
collect_record_stat()
{
    int    i, n_item, ret, no_p_hdr = 1;
    U_64  *tmp, array[MAX_COL_NUM] = {0};
    struct module *mod = NULL;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;
        }

        memset(array, 0, sizeof(array));
        mod->st_flag = 0;
        ret = 0;

        if ((n_item = get_strtok_num(mod->record, ITEM_SPLIT))) {
            /* not merge mode, and last n_item != cur n_item, then reset mod->n_item and set reprint header flag */
            if (MERGE_ITEM != conf.print_merge && n_item && n_item != mod->n_item) {
                no_p_hdr = 0;
                /* reset struct module fields */
                realloc_module_array(mod, n_item);
            }

            mod->n_item = n_item;
            /* multiply item because of have ITEM_SPLIT */
            if (strstr(mod->record, ITEM_SPLIT)) {
                /* merge items */
                if (MERGE_ITEM == conf.print_merge) {
                    mod->n_item = 1;
                    ret = merge_mult_item_to_array(mod->cur_array, mod);

                } else {
                    char item[LEN_1M] = {0};
                    int num = 0;
                    int pos = 0;

                    while (strtok_next_item(item, mod->record, &pos)) {
                        if (!(ret=convert_record_to_array(&mod->cur_array[num * mod->n_col], mod->n_col, item))) {
                            break;
                        }
                        memset(item, 0, sizeof(item));
                        num++;
                    }
                }

            } else { /* one item */
                ret = convert_record_to_array(mod->cur_array, mod->n_col, mod->record);
            }

            /* get st record */
            if (no_p_hdr && mod->pre_flag && ret) {
                set_st_record(mod);
            }

            if (!ret) {
                mod->pre_flag = 0;

            } else {
                mod->pre_flag = 1;
            }

        } else {
            mod->pre_flag = 0;
        }
        /* swap cur_array to pre_array */
        tmp = mod->pre_array;
        mod->pre_array = mod->cur_array;
        mod->cur_array = tmp;
    }

    return no_p_hdr;
}


/*
 * free module info
 */
void
free_modules()
{
    int    i;
    struct module *mod;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (mod->lib) {
            dlclose(mod->lib);
        }

        if (mod->cur_array) {
            free(mod->cur_array);
            mod->cur_array = NULL;
            free(mod->pre_array);
            mod->pre_array = NULL;
            free(mod->st_array);
            mod->st_array = NULL;
        }

        if (mod->max_array) {
            free(mod->max_array);
            free(mod->mean_array);
            free(mod->min_array);
            mod->max_array = NULL;
            mod->mean_array = NULL;
            mod->min_array = NULL;
        }
    }
}


/*
 * read line from file to mod->record
 */
void
read_line_to_module_record(char *line)
{
    int    i;
    struct module *mod;
    char  *s_token, *e_token;
    char   mod_opt[LEN_64];

    line[strlen(line) - 1] = '\0';
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (mod->enable) {
            sprintf(mod_opt, "%s%s%s", SECTION_SPLIT, mod->opt_line, STRING_SPLIT);
            memset(mod->record, 0, sizeof(mod->record));

            s_token = strstr(line, mod_opt);
            if (!s_token) {
                continue;
            }

            s_token += sizeof(SECTION_SPLIT) + strlen(mod->opt_line) + sizeof(STRING_SPLIT) - 2;
            e_token = strstr(s_token, SECTION_SPLIT);

            if (e_token) {
                memcpy(mod->record, s_token, e_token - s_token);

            } else {
                memcpy(mod->record, s_token, strlen(line) - (s_token - line));
            }
        }
    }
}


/*
 * if col num is zero then disable module
 */
void
disable_col_zero()
{
    int    i, j;
    struct module *mod = NULL;
    int    p_col;
    struct mod_info *info;

    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;
        }

        if (!mod->n_col) {
            mod->enable = 0;

        } else {
            p_col = 0;
            info = mod->info;

            for (j = 0; j < mod->n_col; j++) {
                if (((DATA_SUMMARY == conf.print_mode) && (SUMMARY_BIT == info[j].summary_bit))
                        || ((DATA_DETAIL == conf.print_mode) && (HIDE_BIT != info[j].summary_bit))) {
                    p_col++;
                    break;
                }
            }

            if (!p_col) {
                mod->enable = 0;
            }
        }
    }
}


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
lua_set_mod(struct module *mod) {
    lua_State *L = mod->vm;

    lua_newtable(L);
    lua_pushstring(L, "usage");
    lua_pushstring(L, mod->usage);
    lua_settable(L, -3);

    lua_pushstring(L, "opt");
    lua_pushstring(L, mod->opt_line);
    lua_settable(L, -3);

    lua_pushstring(L, "n_col");
    lua_pushinteger(L, mod->n_col);
    lua_settable(L, -3);
}


void
lua_module_set_st_record_wrapper(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    lua_State *L = mod->vm;

    int        i;

    lua_getglobal(L, "set");

    lua_set_mod(mod);

    lua_newtable(L);
    for (i = 0; i < mod->n_col; i++) {
        lua_pushnumber(L, st_array[i]);
        lua_rawseti(L, -2, i+1);
    }

    lua_newtable(L);
    for (i = 0; i < mod->n_col; i++) {
        lua_pushnumber(L, pre_array[i]);
        lua_rawseti(L, -2, i+1);
    }

    lua_newtable(L);
    for (i = 0; i < mod->n_col; i++) {
        lua_pushnumber(L, cur_array[i]);
        lua_rawseti(L, -2, i+1);
    }

    lua_pushnumber(L, inter);

    if (lua_pcall(L, 5, 3, 0) != 0) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper call set() err %s\n", lua_tostring(L, -1));
        return;
    }

    if (!lua_istable(L, -1)) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper set() return 3rd nontable\n");
        return;
    }

    if (!lua_istable(L, -2)) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper set() return 2nd nontable\n");
        return;
    }

    if (!lua_istable(L, -3)) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper set() return 1st nontable\n");
        return;
    }

    for (i = 0; i < mod->n_col; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (!lua_isnumber(L, -1)) {
            continue;
        }
        cur_array[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    for (i = 0; i < mod->n_col; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (!lua_isnumber(L, -1)) {
            continue;
        }
        pre_array[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    for (i = 0; i < mod->n_col; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (!lua_isnumber(L, -1)) {
            continue;
        }
        st_array[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, lua_gettop(L));
}


void
lua_module_data_collect_wrapper(struct module *mod, char *parameter)
{
    lua_State *L = mod->vm;

    lua_getglobal(L,"read");

    lua_set_mod(mod);
    lua_pushstring(L, parameter);

    if (lua_pcall(L, 2, 1, 0) != 0) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper call read() err %s\n", lua_tostring(L, -1));
        return;
    }

    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper %s read function isnt string\n", mod->name);
        return;
    }

    set_mod_record(mod, lua_tostring(L, -1));
}


/*
 * load lua module
 */
void
load_lua_module(struct module *mod) {

    char       buff[LEN_128] = {0};
    char       mod_path[LEN_128] = {0};
    lua_State *L = NULL;

    int        i;

    /* get the full path of modules */
    sprintf(buff, "/usr/local/tsar/modules");

    if (mod->vm) {
        return;
    }

    L = luaL_newstate();
    mod->vm = L;
    luaL_openlibs(L);

    snprintf(mod_path, LEN_128, "%s/%s.lua", buff, mod->name);
    if (luaL_loadfile(L, mod_path) != 0) {
        do_debug(LOG_ERR, "load_lua_module: luaL_loadfile module %s err %s\n",
            mod->name, lua_tostring(L, -1));
        return;
    }
    if (lua_pcall(L, 0, 0, 0) != 0) {
        do_debug(LOG_ERR, "load_lua_module: lua_pcall module %s err %s\n",
            mod->name, lua_tostring(L, -1));
        return;
    }

    lua_getglobal(L,"register");
    if (lua_pcall(L, 0, 1, 0) != 0) {
        do_debug(LOG_ERR, "load_lua_module get register err %s\n", lua_tostring(L, -1));
        return;
    }

    lua_getfield(L, -1, "opt");
    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module opt isn't string\n");
        return;
    }
    sprintf(mod->opt_line, "%s", lua_tostring(L, -1));
    do_debug(LOG_DEBUG, "load_lua_module opt:%s\n", mod->opt_line);
    lua_pop(L, 1);

    lua_getfield(L, -1, "usage");
    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module usage isn't string\n");
        return;
    }
    sprintf(mod->usage, "%s", lua_tostring(L, -1));
    do_debug(LOG_DEBUG, "load_lua_module usage:%s\n", mod->usage);
    lua_pop(L, 1);

    lua_getfield(L, -1, "n_col");
    if (!lua_isnumber(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module n_col isn't number\n");
        return;
    }
    mod->n_col = lua_tointeger(L, -1);
    do_debug(LOG_DEBUG, "load_lua_module n_col:%d\n", mod->n_col);
    lua_pop(L, 1);

    lua_getfield(L, -1, "info");
    if (!lua_istable(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module info isn't table\n");
        return;
    }

    int info_len = lua_objlen(L, -1);
    if (info_len == 0) {
        return;
    }

    if (mod->info) {
        do_debug(LOG_ERR, "load_lua_module duplicated malloc info\n");
    }

    mod->info = malloc(info_len * sizeof(struct mod_info));
    if (!mod->info) {
        do_debug(LOG_ERR, "load_lua_module malloc info error\n");
        return;
    }

    for (i = 0; i < info_len; i ++) {
        lua_rawgeti(L, -1, i + 1); // lua begin from 1
        if (!lua_istable(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info[%d] isn't table\n", i);
            return;
        }
        lua_getfield(L, -1, "hdr");
        if (!lua_isstring(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.hdr isn't string\n");
            return;
        }
        sprintf(mod->info[i].hdr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "summary_bit");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.summary_bit isn't number\n");
            return;
        }
        mod->info[i].summary_bit = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "merge_mode");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.merge_mode isn't number\n");
            return;
        }
        mod->info[i].merge_mode = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "stats_opt");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.stats_opt isn't number\n");
            return;
        }
        mod->info[i].stats_opt = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 1);
        do_debug(LOG_DEBUG, "load_lua_module hdr:%s summary_bit:%d, merge_mode:%d, stats_opt:%d\n",
            mod->info[i].hdr, mod->info[i].summary_bit, mod->info[i].merge_mode, mod->info[i].stats_opt);
    }

    // empty stack
    lua_pop(L, 2);

    mod->data_collect = lua_module_data_collect_wrapper;

    lua_getglobal(L, "set");
    if (lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        mod->set_st_record = lua_module_set_st_record_wrapper;
    }

    mod->enable = 1;
    mod->spec = 0;
    do_debug(LOG_DEBUG, "load_modules: load new module '%s' to mods\n", mod_path);

    return;
}


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
        memset(mod_path, '\0', LEN_128);

        if (strncmp(MOD_LUA_PREFIX, mod->name, strlen(MOD_LUA_PREFIX)) == 0) {
            do_debug(LOG_DEBUG, "load_modules: ready to load %s\n", mod->name);

            if (!mod->vm) {
                load_lua_module(mod);
            }
            continue;
        }

        if (!mod->lib) {
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

        if (mod->vm) {
            lua_close(mod->vm);
            mod->vm = NULL;
            if (mod->info) {
                free(mod->info);
                mod->info = NULL;
            }
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



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


static void
lua_set_mod(lua_State *L, struct module *mod)
{
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


static void
inject_lua_package_path(lua_State *L)
{
    const char *old_path;
    char new_path[LEN_512];

    if (strlen(conf.lua_path)) {
        lua_getglobal(L, "package");
        /* get original package.path */
        lua_getfield(L, -1, "path");
        old_path = lua_tostring(L, -1);
        do_debug(LOG_DEBUG, "old lua package path: %s\n", old_path);
        lua_pop(L, 1);

        sprintf(new_path, "%s;%s", conf.lua_path, old_path);
        do_debug(LOG_DEBUG, "new lua package path: %s\n", new_path);
        lua_pushstring(L, new_path);
        lua_setfield(L, -2, "path");
    }

    if (strlen(conf.lua_cpath)) {
        lua_getglobal(L, "package");
        /* get original package.path */
        lua_getfield(L, -1, "cpath");
        old_path = lua_tostring(L, -1);
        do_debug(LOG_DEBUG, "old lua package cpath: %s\n", old_path);
        lua_pop(L, 1);

        sprintf(new_path, "%s;%s", conf.lua_cpath, old_path);
        do_debug(LOG_DEBUG, "new lua package cpath: %s\n", new_path);
        lua_pushstring(L, new_path);
        lua_setfield(L, -2, "cpath");
    }

    lua_pop(L, 2);
}


static void
inject_tsar_consts(lua_State *L)
{
    lua_pushinteger(L, HIDE_BIT);
    lua_setfield(L, -2, "HIDE_BIT");

    lua_pushinteger(L, DETAIL_BIT);
    lua_setfield(L, -2, "DETAIL_BIT");

    lua_pushinteger(L, SUMMARY_BIT);
    lua_setfield(L, -2, "SUMMARY_BIT");

    lua_pushinteger(L, SPEC_BIT);
    lua_setfield(L, -2, "SPEC_BIT");

    lua_pushinteger(L, MERGE_NULL);
    lua_setfield(L, -2, "MERGE_NULL");

    lua_pushinteger(L, MERGE_SUM);
    lua_setfield(L, -2, "MERGE_SUM");

    lua_pushinteger(L, MERGE_AVG);
    lua_setfield(L, -2, "MERGE_AVG");

    lua_pushinteger(L, STATS_NULL);
    lua_setfield(L, -2, "STATS_NULL");

    lua_pushinteger(L, STATS_SUB);
    lua_setfield(L, -2, "STATS_SUB");

    lua_pushinteger(L, STATS_SUB_INTER);
    lua_setfield(L, -2, "STATS_SUB_INTER");
}


static void
inject_tsar_api(lua_State *L)
{
    /* tsar */
    lua_newtable(L);

    inject_tsar_consts(L);

    lua_setglobal(L, "tsar");
}


static int
load_lua_module_info(lua_State *L, struct module *mod)
{
    int i = 0;
    int info_len = 0;


    lua_getfield(L, -1, "info");
    if (!lua_istable(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module info isn't table\n");
        return 1;
    }

    info_len = lua_objlen(L, -1);
    if (info_len == 0) {
        return 0;
    }
    mod->n_col = info_len;

    if (mod->info) {
        do_debug(LOG_ERR, "load_lua_module duplicated malloc info\n");
        return 1;
    }

    mod->info = malloc(info_len * sizeof(struct mod_info));
    if (!mod->info) {
        do_debug(LOG_ERR, "load_lua_module malloc info error\n");
        return 1;
    }

    for (i = 0; i < info_len; i ++) {
        lua_rawgeti(L, -1, i + 1); // lua begin from 1
        if (!lua_istable(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info[%d] isn't table\n", i);
            return 1;
        }
        lua_getfield(L, -1, "hdr");
        if (!lua_isstring(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.hdr isn't string\n");
            return 1;
        }
        sprintf(mod->info[i].hdr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "summary_bit");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.summary_bit isn't number\n");
            return 1;
        }
        mod->info[i].summary_bit = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "merge_mode");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.merge_mode isn't number\n");
            return 1;
        }
        mod->info[i].merge_mode = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "stats_opt");
        if (!lua_isnumber(L, -1)) {
            do_debug(LOG_ERR, "load_lua_module info.stats_opt isn't number\n");
            return 1;
        }
        mod->info[i].stats_opt = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 1);
        do_debug(LOG_DEBUG, "load_lua_module hdr:%s summary_bit:%d, merge_mode:%d, stats_opt:%d\n",
            mod->info[i].hdr, mod->info[i].summary_bit, mod->info[i].merge_mode, mod->info[i].stats_opt);
    }

    lua_pop(L, 2);

    return 0;
}


static int
load_lua_module_optusage(lua_State *L, struct module *mod)
{
    lua_getfield(L, -1, "opt");
    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module opt isn't string\n");
        return 1;
    }
    sprintf(mod->opt_line, "%s", lua_tostring(L, -1));
    do_debug(LOG_DEBUG, "load_lua_module opt:%s\n", mod->opt_line);
    lua_pop(L, 1);

    lua_getfield(L, -1, "usage");
    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "load_lua_module usage isn't string\n");
        return 1;
    }
    sprintf(mod->usage, "    %-20s%s", mod->opt_line, lua_tostring(L, -1));
    do_debug(LOG_DEBUG, "load_lua_module usage:%s\n", mod->usage);
    lua_pop(L, 1);

    return 0;
}


static int
load_lua_module_register(lua_State *L, struct module *mod)
{
    lua_getfield(L, -1, "register");
    if (lua_pcall(L, 0, 1, 0) != 0) {
        do_debug(LOG_ERR, "load_lua_module call _M.register() err %s\n", lua_tostring(L, -1));
        return 1;
    }

    if (load_lua_module_optusage(L, mod) != 0) {
        return 1;
    }

    if (load_lua_module_info(L, mod) != 0) {
        return 1;
    }

    return 0;
}


void
close_luavm(lua_State *L)
{
    lua_close(L);
}


lua_State *
load_luavm()
{
    lua_State *vm;

    vm = luaL_newstate();
    if (vm == NULL) {
        return NULL;
    }

    luaL_openlibs(vm);

    inject_lua_package_path(vm);
    inject_tsar_api(vm);

    return vm;
}


static void
lua_module_set_st_record_wrapper(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int        i;

    lua_getglobal(L, mod->name);
    if (!lua_istable(L, -1)) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper %s isn's table\n", mod->name);
        return;
    }

    lua_getfield(L, -1, "set");
    if (lua_isnil(L, -1)) {
        do_debug(LOG_DEBUG, "lua_module_set_st_record_wrapper %s.set() doesnt set\n", mod->name);
        for (i = 0; i < mod->n_col; i++) {
            st_array[i] = cur_array[i] - pre_array[i];
        }
        return;
    } else if (!lua_isfunction(L, -1)) {
        do_debug(LOG_ERR, "lua_module_set_st_record_wrapper %s.set() isnt function\n", mod->name);
        return;
    }

    lua_set_mod(L, mod);

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


static void
lua_module_data_collect_wrapper(struct module *mod, char *parameter)
{
    lua_getglobal(L, mod->name);
    if (!lua_istable(L, -1)) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper %s isn's table\n", mod->name);
        return;
    }

    lua_getfield(L, -1, "read");
    if (lua_isnil(L, -1)) {
        do_debug(LOG_DEBUG, "lua_module_data_collect_wrapper %s.read() doesnt set\n", mod->name);
        return;
    } else if (lua_isfunction(L, -1) == 0) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper %s.read() isnt function\n", mod->name);
        return;
    }

    lua_set_mod(L, mod);
    lua_pushstring(L, parameter);

    if (lua_pcall(L, 2, 1, 0) != 0) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper call %s.read() err %s\n", mod->name, lua_tostring(L, -1));
        return;
    }

    if (!lua_isstring(L, -1)) {
        do_debug(LOG_ERR, "lua_module_data_collect_wrapper %s.read() function return value isnt string\n", mod->name);
        return;
    }

    set_mod_record(mod, lua_tostring(L, -1));
}


void
load_lua_module(lua_State *L, struct module *mod)
{
    char       buff[LEN_128] = {0};
    char       mod_path[LEN_128] = {0};

    /* get the full path of modules */
    sprintf(buff, DEFAULT_MODULE_PATH);

    snprintf(mod_path, LEN_128, "%s/%s.lua", buff, mod->name);

    if (luaL_loadfile(L, mod_path) != 0) {
        do_debug(LOG_ERR, "load_lua_module: luaL_loadfile module %s err %s\n",
            mod->name, lua_tostring(L, -1));
        return;
    }

    if (lua_pcall(L, 0, 1, 0) != 0) {
        do_debug(LOG_ERR, "load_lua_module: lua_pcall module %s err %s\n",
            mod->name, lua_tostring(L, -1));
        return;
    }

    if (load_lua_module_register(L, mod) != 0) {
        return;
    }

    mod->data_collect = lua_module_data_collect_wrapper;
    mod->set_st_record = lua_module_set_st_record_wrapper;

    // load lua module to global
    lua_setglobal(L, mod->name);

    mod->enable = 1;
    mod->spec = 0;
    do_debug(LOG_DEBUG, "load_modules: load new module '%s' to mods\n", mod_path);

    return;
}

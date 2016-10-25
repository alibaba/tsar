
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


#ifndef TSAR_LUA_UTIL_H
#define TSAR_LUA_UTIL_H


#include "define.h"

void inject_lua_package_path(lua_State *L);
void inject_tsar_consts(lua_State *L);
void inject_tsar_api(lua_State *L);
void close_luavm(lua_State *L);
lua_State *load_luavm();
void lua_set_mod(lua_State *L, struct module *mod);
void load_lua_module(lua_State *L, struct module *mod);
void lua_module_set_st_record_wrapper(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter);
void lua_module_data_collect_wrapper(struct module *mod, char *parameter);

#endif

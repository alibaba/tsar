
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


#ifndef _TSARMOD_H
#define _TSARMOD_H


#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/stat.h>


#define U_64        unsigned long long

#define LEN_32      32
#define LEN_64      64
#define LEN_128     128
#define LEN_256     256
#define LEN_512     512
#define LEN_1024    1024
#define LEN_4096    4096
#define LEN_1M      1048576

#define ITEM_SPLIT      ";"
#define DATA_SPLIT      ","


struct mod_info {
    char   hdr[LEN_128];
    int    summary_bit;    /* bit set indefi summary */
    int    merge_mode;
    int    stats_opt;
};

struct module {

    char    name[LEN_32];
    char    opt_line[LEN_32];
    char    record[LEN_1M];
    char    usage[LEN_256];
    char    parameter[LEN_256];
    char    print_item[LEN_256];

    struct  mod_info *info;
    void   *lib;
    int     enable;
    int     spec;
    int     p_item;

    /* private data used by framework*/
    int     n_item;
    int     n_col;
    long    n_record;

    int     pre_flag:4;
    int     st_flag:4;

    U_64   *pre_array;
    U_64   *cur_array;
    double *st_array;
    double *max_array;
    double *mean_array;
    double *min_array;

    /* callback function of module */
    void (*data_collect) (struct module *, char *);
    void (*set_st_record) (struct module *, double *, U_64 *, U_64 *, int);

    /* mod manage */
    void (*mod_register) (struct module *);
};

void register_mod_fields(struct module *mod, const char *opt, const char *usage,
        struct mod_info *info, int n_col, void *data_collect, void *set_st_record);
void set_mod_record(struct module *mod, const char *record);

enum {
    HIDE_BIT,
    DETAIL_BIT,
    SUMMARY_BIT,
    SPEC_BIT
};

enum {
    MERGE_NULL,
    MERGE_SUM,
    MERGE_AVG
};

enum {
    STATS_NULL,
    STATS_SUB,
    STATS_SUB_INTER
};

#endif

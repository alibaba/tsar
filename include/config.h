
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


#ifndef TSAR_CONFIG_H
#define TSAR_CONFIG_H


#include "define.h"


struct configure {

    /* from arg */
    int     running_mode;               /* running mode */
    char    config_file[LEN_128];
    int     debug_level;

    char    output_interface[LEN_128];  /* which interface will enable*/

    /* output print */
    char    output_print_mod[LEN_512];  /* which mod will print throught argv */
    char    output_stdio_mod[LEN_512];  /* which mod will print throuhth conf file */
    char    output_nagios_mod[LEN_512]; /* which mod will output to nagios */
    int     print_interval;             /* how many seconds will escape every print interval */
    int     print_nline_interval;       /* how many lines will skip every print interval */
    int     print_mode;                 /* data type will print: summary or detail */
    int     print_merge;                /* mult items is merge */
    int     print_detail;               /* conver data to K/M/G */
    int     print_ndays;                /* these days will print.default:1 */
    int     print_day;                  /* which day will print */
    int     print_start_time;           /* the start of the print time */
    int     print_end_time;             /* the end of the print time */
    int     print_tail;
    int     print_file_number;          /* which tsar.data file used */
    int     print_max_day;              /* max day for history print */

    /* output db */
    char    output_db_mod[LEN_512];     /* which mod will output */
    char    output_db_addr[LEN_512];    /* db addr */

    /* output tcp */
    char    output_tcp_mod[LEN_512];
    char    output_tcp_addr[LEN_256];
    char    output_tcp_merge[LEN_256];

    /* output nagios */
    char    server_addr[LEN_512];
    int     server_port;
    int     cycle_time;
    char    send_nsca_cmd[LEN_512];
    char    send_nsca_conf[LEN_512];

    char    check_name[MAX_MOD_NUM][LEN_32];
    float   wmin[MAX_MOD_NUM];
    float   wmax[MAX_MOD_NUM];
    float   cmin[MAX_MOD_NUM];
    float   cmax[MAX_MOD_NUM];
    int     mod_num;

    /* output file */
    char    output_file_path[LEN_128];
};


void parse_config_file(const char *file_name);
void get_include_conf();
void get_threshold();
void set_special_field(const char *spec_field);
void set_special_item(const char *spec_field);


#endif

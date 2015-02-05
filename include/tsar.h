
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


#ifndef TSAR_H
#define TSAR_H


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

#include "framework.h"
#include "debug.h"
#include "config.h"

#include "output_file.h"
#include "output_print.h"
#include "output_db.h"
#include "output_tcp.h"
#include "output_nagios.h"
#include "common.h"


struct statistic {
    int    total_mod_num;
    time_t cur_time;
};


extern struct configure conf;
extern struct module    mods[MAX_MOD_NUM];
extern struct statistic statis;


#endif

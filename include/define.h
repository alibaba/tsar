
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


#ifndef TSAR_DEFINE_H
#define TSAR_DEFINE_H


//-check & --check function for old tsar amon usage
#define OLDTSAR

#define U_BIT       3

#define U_64        unsigned long long

#define LEN_32      32
#define LEN_64      64
#define LEN_128     128
#define LEN_256     256
#define LEN_512     512
#define LEN_1024    1024
#define LEN_4096    4096
#define LEN_1M      1048576
#define LEN_10M     10485760

#define MAX_COL_NUM 64
#define MAX_MOD_NUM 32

#define SECTION_SPLIT   "|"
#define STRING_SPLIT    ":"
#define ITEM_SPLIT  ";"
#define ITEM_SPSTART    "="
#define DATA_SPLIT  ","
#define PARAM_SPLIT ':'
#define HDR_SPLIT   "#"
#define PRINT_DATA_SPLIT "  "
#define PRINT_SEC_SPLIT " "
#define W_SPACE     " \t\r\n"

#define DEFAULT_PRINT_NUM   20
#define DEFAULT_PRINT_INTERVAL  5

#define MOD_INFO_SIZE       sizeof(strcut mod_info)

#define DEFAULT_CONF_FILE_PATH      "/etc/tsar/tsar.conf"
#define DEFAULT_OUTPUT_FILE_PATH    "/var/log/tsar.data"
#define MIN_STRING "MIN:        "
#define MEAN_STRING "MEAN:       "
#define MAX_STRING "MAX:        "

#define TRUE 1
#define FALSE 0

#define VMSTAT "/proc/vmstat"
#define STAT "/proc/stat"
#define MEMINFO "/proc/meminfo"
#define LOADAVG "/proc/loadavg"
#define NET_DEV "/proc/net/dev"
#define NET_SNMP "/proc/net/snmp"
#define APACHERT "/tmp/apachert.mmap"
#define TCP "/proc/net/tcp"
#define NETSTAT "/proc/net/netstat"


enum {
    MERGE_NOT,
    MERGE_ITEM
};


enum {
    RUN_NULL,
    RUN_LIST,
    RUN_CRON,
#ifdef OLDTSAR
    RUN_CHECK,
    RUN_CHECK_NEW,
#endif
    RUN_PRINT,
    RUN_PRINT_LIVE
};


enum {
    DATA_NULL,
    DATA_SUMMARY,
    DATA_DETAIL,
    DATA_ALL
};


enum {
    TAIL_NULL,
    TAIL_MAX,
    TAIL_MEAN,
    TAIL_MIN
};


enum {
    OUTPUT_NULL,
    OUTPUT_PRINT,
    OUTPUT_NAGIOS
};


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

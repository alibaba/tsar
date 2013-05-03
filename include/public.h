
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


#ifndef TSAR_PUBLIC_H
#define TSAR_PUBLIC_H


#include <errno.h>
#include <math.h>
#include <ctype.h>
#include "tsar.h"


/*
 * /proc/files
 */
#define NET_DEV "/proc/net/dev"
#define STAT "/proc/stat"
#define MEMINFO "/proc/meminfo"
#define LOADAVG "/proc/loadavg"
#define INTERRUPT "/proc/interrupts"
#define APACHERT "/tmp/apachert.mmap"
#define VMSTAT "/proc/vmstat"
#define DISKSTATS "/proc/diskstats"
#define NET_SNMP "/proc/net/snmp"
#define FDENTRY_STATE   "/proc/sys/fs/dentry-state"
#define FFILE_NR        "/proc/sys/fs/file-nr"
#define FINODE_STATE    "/proc/sys/fs/inode-state"
#define PTY_NR          "/proc/sys/kernel/pty/nr"


/*
 *  ANSI Color setting segment
 *
 */
#define BLUE_FMT(s) "\033[40;34m"s"\033[0m"
#define GREEN_FMT(s) "\033[40;32m"s"\033[0m"
#define RED_FMT(s) "\033[40;31m"s"\033[0m"


#define COLOR(val, fmt, str, ret, color) do             \
{                           \
    if ((val) > 100)                \
    ret = sprintf(str,          \
            color##_FMT(fmt), (val));   \
    else                        \
    ret = sprintf(str, fmt, (val));     \
} while(0)

#define FALSE 0
#define TRUE 1
#define PRINT_STATISTICS FALSE

#define CURR 0
#define PAST 1


/*
 * for statistics
 */
#define NR_ARRAY 3

enum {MIN, MEAN, MAX};


/*
 * Macros used to display statistics values.
 *
 * NB: Define SP_VALUE() to normalize to %;
 * HZ is 1024 on IA64 and % should be normalized to 100.
 */
#define _S(a, b) (((b) == 0) ? 0 : ((a) / (b)))

#define S_VALUE(m, n, p)  (((double) ((n) - (m))) / (p) )

#define SP_VALUE(m, n, p) (((double) ((n) - (m))) / (p) * 100)

#define SK(K, shift) (((unsigned long long)(K) << 10) >> shift)
#define SB(B, shift) ((unsigned long long)(B) >> shift)
#define KTOB(B) ((unsigned long long)(B) << 10)

#define FMT_U64 "%lld"
#define FMT_U32 "%ld"

#define FMT_SP " %5.1f "
#define FMT_S "%5.1f%c "
#define FMT_NAGIOS "%.2f,"

/*1000 * 1024 * 1024 * 1024*/
#define TB 0xF424000000

/* 1000 * 1024 * 1024*/
#define GB 0x3E800000

/* 1000 * 1024 */
#define MB 0xFA000

/* 1000 */
#define KB 0x3E8

#define __INDENT(f, idx, ret)               \
    do {                        \
        if((f/1024.0) > GB) {           \
            (ret) = (f) * 1.0/1024.0 / GB;  \
            (idx) = 3;          \
        }                   \
        else if((f) > GB) {         \
            (ret) = (f) * 1.0 / GB;     \
            (idx) = 2;          \
        }                   \
        else if ((f) > MB){         \
            (ret) = (f) * 1.0 / MB;     \
            (idx) = 1;          \
        }                   \
        else {                  \
            (ret) = (f) * 1.0 / KB;     \
            (idx) = 0;          \
        }                   \
    }while(0)


#define __PRINT_S(buffer, val, ret) do {            \
    int _i;                     \
    double _f;                  \
    char u[] = {'K', 'M', 'G', 'T'};        \
    __INDENT((val), _i, _f);            \
    (ret) += sprintf((buffer), FMT_S, _f, u[_i]);   \
}while(0)

#define __PRINT_SP(buffer, val, ret) do {           \
    (ret) += sprintf(buffer, FMT_SP, (val));    \
}while(0)

#define __PRINT_NAGIOS(buffer, val, ret) do {           \
    (ret) += sprintf(buffer, FMT_NAGIOS, (val));    \
}while(0)

#define PRINT(buffer, val, ret, type) do {          \
    if (type == OUTPUT_NAGIOS) {            \
        __PRINT_NAGIOS(buffer, val, ret);   \
    }                       \
    else{                       \
        if ((val) < KB) {           \
            __PRINT_SP(buffer, val, ret);   \
        }                   \
        else {                  \
            __PRINT_S(buffer, val, ret);    \
        }                   \
    }                       \
} while(0)

#define myalloc(p, type, size)                  \
    struct type * p = NULL;                 \
p = (struct type *)malloc(size);            \
if(!p) {                        \
    fprintf(stderr, "failed to alloc memory\n");    \
    exit(EXIT_FAILURE);             \
}                           \
memset(p, 0, size)                  \

#define BUFFER_ROTATE(mod, size)                    \
    do {                                \
        memcpy(&s_st_##mod[PAST], &s_st_##mod[CURR], (size));   \
        memset(&s_st_##mod[CURR], '\0', (size));        \
    }while(0)

inline void func_mod_free(struct module *mod)
{
    free(mod->detail);
    free(mod->summary);
    mod->detail = NULL;
    mod->summary = NULL;
}

#define INIT_STRING_P(s, nr, len)               \
    do {                            \
        int i;                      \
        s = (char **)malloc((nr) * sizeof(char *)); \
        for(i = 0; i < (nr); i++)           \
        s[i] = (char *)malloc(len);     \
    } while(0)


#define DECLARE_TMP_MOD_STATISTICS(mod)     \
    union mod##_statistics mod##_tmp_s; \

#define SET_CURRENT_VALUE(mod, type, member, ret)   \
    do {                        \
        mod##_tmp_s.mod##_##type.ret =      \
        s_st_##mod[CURR].member;    \
    }while(0)

#define __COMPUTE_MOD_VALUE(ret, ops, m1, m2, i)    \
    do {                        \
        if (!(i)) {             \
            (ret) = 0;          \
        }                   \
        else if ((m1) == (m2))  {       \
            (ret) = 0;          \
        }                   \
        else {                  \
            (ret) = ops((m1), (m2), (i));   \
        }                   \
    } while(0)


#define COMPUTE_MOD_VALUE(mod, ops, type, member, i, ret)       \
    do {                                \
        /*printf("i = %ld\n", (i));*/               \
        __COMPUTE_MOD_VALUE(                    \
                mod##_tmp_s.mod##_##type.ret,   \
                ops,                \
                s_st_##mod[1].member,       \
                s_st_##mod[0].member,       \
                (i));               \
    } while(0)


/* Fix me */
#define __SET_MOD_STATISTICS(val, mean, max, min, i)        \
    do{                         \
        static int sw = 0;              \
        if(!sw) {                   \
            (max) = (val);              \
            (min) = (val);              \
            sw = 1;                 \
        } else {                    \
            if (((val) - (max)) > 0.00001)      \
            (max) = (val);          \
            else if (((min) - (val)) > 0.00001) {   \
                (min) = (val);          \
            }                   \
        }                       \
        (mean) += (val);                \
    } while(0)

#define SET_MOD_STATISTICS(mod, member, i, type)    \
    __SET_MOD_STATISTICS                \
(                       \
                        mod##_tmp_s.mod##_##type.member,        \
                        mod##_statis[MEAN].mod##_##type.member, \
                        mod##_statis[MAX].mod##_##type.member,      \
                        mod##_statis[MIN].mod##_##type.member,      \
                        i)

#define __PRINT_AVG(buf, pos, val, member, idx, count, otype) do    \
{                           \
    if ((idx) == MEAN)              \
    val[(idx)].member =         \
    val[(idx)].member / (count);    \
    PRINT(buf[(idx)] + pos[(idx)],          \
            val[(idx)].member,          \
            pos[(idx)], (otype));           \
}while(0)

#define __PRINT_AVG_SEP(buf, pos, val, member, sep, idx, count, otype) do \
{                           \
    if((idx) == MEAN)               \
    val[(idx)].member =         \
    (val[(idx)].member / (count));  \
    PRINT(buf[(idx)] + pos[(idx)],          \
            val[(idx)].member * (sep),      \
            pos[(idx)], (otype));           \
}while(0)

inline char *getitem(char *r, char *mnt)
{
    char *start, *end;
    if (r == NULL || *r == '\0') {
        return NULL;
    } else {
        start = strstr(r, "=");
        end = strstr(r, ";");
        memcpy(mnt, start + 1, end - start -1);
        r = end + 1;
        mnt[end - start - 1] = '\0';
    }

    return r;
}

#define CALITV(pt, ct, i) ((i) = ((pt) < (ct)) ? (ct) - (pt) : 1)


#endif

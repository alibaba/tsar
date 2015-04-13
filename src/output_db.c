
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


/*
 * send sql to remote db
 */
void
send_sql_txt(int fd, int have_collect)
{
    int         i = 0, j, len;
    char        s_time[LEN_64] = {0};
    char        host_name[LEN_64] = {0};
    struct      module *mod;
    static char sqls[LEN_10M] = {0};

    /* get hostname */
    if (0 != gethostname(host_name, sizeof(host_name))) {
        do_debug(LOG_FATAL, "send_sql_txt: gethostname err, errno=%d", errno);
    }
    while (host_name[i]) {
        if (!isprint(host_name[i++])) {
            host_name[i-1] = '\0';
            break;
        }
    }

    /* get st_array */
    if (get_st_array_from_file(have_collect)) {
        return;
    }

    /* only output from output_db_mod */
    reload_modules(conf.output_db_mod);

    sprintf(s_time, "%ld", time(NULL));

    /* print summary data */
    for (i = 0; i < statis.total_mod_num; i++) {
        mod = &mods[i];
        if (!mod->enable) {
            continue;

        } else {
            if (!mod->st_flag) {
                char    sql_hdr[LEN_256] = {0};
                /* set sql header */
                memset(sql_hdr, '\0', sizeof(sql_hdr));
                sprintf(sql_hdr, "insert into `%s` (host_name, time) VALUES ('%s', '%s');",
                        mod->opt_line + 2, host_name, s_time);
                strcat(sqls, sql_hdr);

            } else {
                char    str[LEN_32] = {0};
                char    sql_hdr[LEN_256] = {0};
                struct  mod_info *info = mod->info;

                /* set sql header */
                memset(sql_hdr, '\0', sizeof(sql_hdr));
                sprintf(sql_hdr, "insert into `%s` (host_name, time", mod->opt_line + 2);

                /* get value */
                for (j = 0; j < mod->n_col; j++) {
                    strcat(sql_hdr, ", `");
                    char *p = info[j].hdr;
                    while (*p == ' ') {
                        p++;
                    }
                    strcat(sql_hdr, p);
                    strcat(sql_hdr, "`");
                }
                strcat(sql_hdr, ") VALUES ('");
                strcat(sql_hdr, host_name);
                strcat(sql_hdr, "', '");
                strcat(sql_hdr, s_time);
                strcat(sql_hdr, "'");
                strcat(sqls, sql_hdr);

                /* get value */
                for (j = 0; j < mod->n_col; j++) {
                    memset(str, 0, sizeof(str));
                    sprintf(str, ", '%.1f'", mod->st_array[j]);
                    strcat(sqls, str);
                }
                strcat(sqls, ");");
            }
        }
    }
    len = strlen(sqls);
    if (write(fd, sqls, len) != len) {
        do_debug(LOG_ERR, "output_db write error:%s", strerror(errno));
    }
}

struct sockaddr_in *
str2sa(char *str)
{
    int                       port;
    char                     *c;
    static struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    str = strdup(str);
    if (str == NULL) {
        goto out_nofree;
    }

    if ((c = strrchr(str, ':')) != NULL) {
        *c++ = '\0';
        port = atol(c);

    } else {
        port = 0;
    }

    if (*str == '*' || *str == '\0') { /* INADDR_ANY */
        sa.sin_addr.s_addr = INADDR_ANY;

    } else {
        if (!inet_pton(AF_INET, str, &sa.sin_addr)) {
            struct hostent *he;

            if ((he = gethostbyname(str)) == NULL) {
                do_debug(LOG_FATAL, "str2sa: Invalid server name, '%s'", str);

            } else {
                sa.sin_addr = *(struct in_addr *) *(he->h_addr_list);
            }
        }
    }
    sa.sin_port   = htons(port);
    sa.sin_family = AF_INET;

    free(str);
out_nofree:
    return &sa;
}

void
output_db(int have_collect)
{
    int        fd, flags, res;
    fd_set     fdr, fdw;
    struct     timeval timeout;
    struct     sockaddr_in db_addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        do_debug(LOG_FATAL, "can't get socket");
    }

    /* set socket fd noblock */
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
        close(fd);
        return;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return;
    }

    /* get db server address */
    db_addr = *str2sa(conf.output_db_addr);

    if (connect(fd, (struct sockaddr*)&db_addr, sizeof(db_addr)) != 0) {
        if (errno != EINPROGRESS) { // EINPROGRESS
            close(fd);
            return;

        } else {
            goto select;
        }

    } else {
        goto send;
    }

select:
    FD_ZERO(&fdr);
    FD_ZERO(&fdw);
    FD_SET(fd, &fdr);
    FD_SET(fd, &fdw);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    res = select(fd + 1, &fdr, &fdw, NULL, &timeout);
    if (res <= 0) {
        close(fd);
        return;
    }

send:
    send_sql_txt(fd, have_collect);
    close(fd);
}

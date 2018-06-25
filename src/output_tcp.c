
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

void
send_data_tcp(char *output_addr, char *data, int len)
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
    db_addr = *str2sa(output_addr);

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
    if (len > 0 && write(fd, data, len) != len) {
        do_debug(LOG_ERR, "output_db write error:dst:%s\terrno:%s\n",output_addr, strerror(errno));
    }
    close(fd);
}

void
output_multi_tcp(int have_collect)
{
    int           out_pipe[2];
    int           len;
    static char   data[LEN_10M] = {0};
    int         i;
    /* only output from output_tcp_mod */
    reload_modules(conf.output_tcp_mod);

    if (!strcasecmp(conf.output_tcp_merge, "on") || !strcasecmp(conf.output_tcp_merge, "enable")) {
        conf.print_merge = MERGE_ITEM;
    } else {
        conf.print_merge = MERGE_NOT;
    }

    if (pipe(out_pipe) != 0) {
        return;
    }

    dup2(out_pipe[1], STDOUT_FILENO);
    close(out_pipe[1]);

    running_check(RUN_CHECK_NEW);

    fflush(stdout);
    len = read(out_pipe[0], data, LEN_10M);
    close(out_pipe[0]);
    /*now ,the data to send is gotten*/
    for(i = 0; i < conf.output_tcp_addr_num; i++){
        send_data_tcp(conf.output_tcp_addr[i], data, len);
    }
}

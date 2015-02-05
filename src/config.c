
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


/* add mod to tsar */
void
parse_mod(const char *mod_name)
{
    int     i = 0;
    struct  module *mod;
    char   *token;

    /* check if the mod load already */
    for ( i = 0; i < statis.total_mod_num; i++ )
    {
        mod = &mods[i];
        if (!strcmp(mod->name, mod_name)) {
            return;
        }
    }
    if (statis.total_mod_num >= MAX_MOD_NUM) {
        do_debug(LOG_ERR, "Max mod number is %d ignore mod %s\n", MAX_MOD_NUM, mod_name);
        return;
    }

    mod = &mods[statis.total_mod_num++];
    token = strtok(NULL, W_SPACE);
    if (token && (!strcasecmp(token, "on") || !strcasecmp(token, "enable"))) {
        strncpy(mod->name, mod_name, strlen(mod_name));
        token = strtok(NULL, W_SPACE);
        if (token) {
            strncpy(mod->parameter, token, strlen(token));
        }

    } else {
        memset(mod, 0, sizeof(struct module));
        statis.total_mod_num--;
    }
}

void
special_mod(const char *spec_mod)
{
    int       i = 0, j = 0;
    char      mod_name[LEN_32];
    struct    module *mod = NULL;

    memset(mod_name, 0, LEN_32);
    sprintf(mod_name, "mod_%s", spec_mod + 5);
    for ( i = 0; i < statis.total_mod_num; i++ )
    {
        mod = &mods[i];
        if (!strcmp(mod->name, mod_name)) {
            /* set special field */
            load_modules();
            char    *token = strtok(NULL, W_SPACE);
            struct mod_info *info = mod->info;
            for (j=0; j < mod->n_col; j++) {
                char *p = info[j].hdr;
                while (*p  == ' ') {
                    p++;
                }
                if (strstr(token, p)) {
                    info[j].summary_bit = SPEC_BIT;
                    mod->spec = 1;
                }
            }
        }
    }
}

void
parse_int(int *var)
{
    char   *token = strtok(NULL, W_SPACE);

    if (token == NULL) {
        do_debug(LOG_FATAL, "Bungled line");
    }
    *var = strtol(token, NULL, 0);
}

void
parse_string(char *var)
{
    char   *token = strtok(NULL, W_SPACE);

    if (token) {
        strncpy(var, token, strlen(token));
    }
}

void
parse_add_string(char *var)
{
    char   *token = strtok(NULL, W_SPACE);

    if (var == NULL) {
        if (token) {
            strncpy(var, token, strlen(token));
        }

    } else {
        if (token) {
            strcat(token, ",");
            strncat(token, var, strlen(var));
        }
        if (token) {
            strncpy(var, token, strlen(token));
        }
    }
}

void
set_debug_level()
{
    char   *token = strtok(NULL, W_SPACE);

    if (token) {
        if (!strcmp(token, "INFO")) {
            conf.debug_level = LOG_INFO;

        } else if (!strcmp(token, "WARN")) {
            conf.debug_level = LOG_WARN;

        } else if (!strcmp(token, "DEBUG")) {
            conf.debug_level = LOG_DEBUG;

        } else if (!strcmp(token, "ERROR")) {
            conf.debug_level = LOG_ERR;

        } else if (!strcmp(token, "FATAL")) {
            conf.debug_level = LOG_FATAL;

        } else {
            conf.debug_level = LOG_ERR;
        }
    }
}

/* parse every config line */
static int
parse_line(char *buff)
{
    char   *token;

    if ((token = strtok(buff, W_SPACE)) == NULL) {
        /* ignore empty lines */
        (void) 0;

    } else if (token[0] == '#') {
        /* ignore comment lines */
        (void) 0;

    } else if (strstr(token, "mod_")) {
        parse_mod(token);

    } else if (strstr(token, "spec_")) {
        special_mod(token);

    } else if (!strcmp(token, "output_interface")) {
        parse_string(conf.output_interface);

    } else if (!strcmp(token, "output_file_path")) {
        parse_string(conf.output_file_path);

    } else if (!strcmp(token, "output_db_addr")) {
        parse_string(conf.output_db_addr);

    } else if (!strcmp(token, "output_db_mod")) {
        parse_add_string(conf.output_db_mod);

    } else if (!strcmp(token, "output_tcp_mod")) {
        parse_add_string(conf.output_tcp_mod);
    } else if (!strcmp(token, "output_tcp_addr")) {
        parse_string(conf.output_tcp_addr);
    } else if (!strcmp(token, "output_tcp_merge")) {
        parse_string(conf.output_tcp_merge);

    } else if (!strcmp(token, "output_nagios_mod")) {
        parse_add_string(conf.output_nagios_mod);

    } else if (!strcmp(token, "output_stdio_mod")) {
        parse_add_string(conf.output_stdio_mod);

    } else if (!strcmp(token, "debug_level")) {
        set_debug_level();

    } else if (!strcmp(token, "include")) {
        get_include_conf();

    } else if (!strcmp(token, "server_addr")) {
        parse_string(conf.server_addr);

    } else if (!strcmp(token, "server_port")) {
        parse_int(&conf.server_port);

    } else if (!strcmp(token, "cycle_time")) {
        parse_int(&conf.cycle_time);

    } else if (!strcmp(token, "max_day")) {
        parse_int(&conf.print_max_day);

    } else if (!strcmp(token, "send_nsca_cmd")) {
        parse_string(conf.send_nsca_cmd);

    } else if (!strcmp(token, "send_nsca_conf")) {
        parse_string(conf.send_nsca_conf);

    } else if (!strcmp(token, "threshold")) {
        get_threshold();

    } else {
        return 0;
    }
    return 1;
}

static void
process_input_line(char *config_input_line, int len, const char *file_name)
{
    char *token;
    
    if ((token = strchr(config_input_line, '\n'))) {
    	*token = '\0';
    }
    if ((token = strchr(config_input_line, '\r'))) {
    	*token = '\0';
    }
    if (config_input_line[0] == '#') {
    	goto final;
    } else if (config_input_line[0] == '\0') {
    	goto final;
    }
    /* FIXME can't support wrap line */
    if (!parse_line(config_input_line)) {
        do_debug(LOG_INFO, "parse_config_file: unknown keyword in '%s' at file %s\n", 
                 config_input_line, file_name);
    }
    
final:
    memset(config_input_line, '\0', LEN_1024);
}

void
parse_config_file(const char *file_name)
{
    FILE   *fp;
    char    config_input_line[LEN_1024] = {0};

    if (!(fp = fopen(file_name, "r"))) {
        do_debug(LOG_FATAL, "Unable to open configuration file: %s", file_name);
    }

    memset(&mods, '\0', sizeof(mods));
    memset(&conf, '\0', sizeof(conf));
    memset(&statis, '\0', sizeof(statis));
    conf.debug_level = LOG_ERR;
    conf.print_detail = FALSE;
    conf.print_max_day = 365;
    sprintf(conf.output_interface, "file");
    while (fgets(config_input_line, LEN_1024, fp)) {
        process_input_line(config_input_line, LEN_1024, file_name);
    }
    if (fclose(fp) < 0) {
        do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
    }
}

/* deal with the include statment */
void
get_include_conf()
{
    char   *token = strtok(NULL, W_SPACE);
    char   *p;
    FILE   *stream, *fp;
    char    cmd[LEN_1024] = {0};
    char    buf[LEN_1024] = {0};
    char    config_input_line[LEN_1024] = {0};

    if (token) {
        memset(cmd, '\0', LEN_1024);
        sprintf(cmd, "ls %s 2>/dev/null", token);
        if (strchr(cmd, ';') != NULL || strchr(cmd, '|') != NULL || strchr(cmd, '&') != NULL) {
            do_debug(LOG_ERR, "include formart Error:%s\n", cmd);
        }
        stream = popen(cmd, "r");
        if (stream == NULL) {
            do_debug(LOG_ERR, "popen failed. Error:%s\n", strerror(errno));
            return;
        }
        memset(buf, '\0', LEN_1024);
        while (fgets(buf, LEN_1024, stream)) {
            do_debug(LOG_INFO, "parse file %s", buf);
            p = buf;
            while (p) {
                if (*p == '\r' || *p == '\n') {
                    *p = '\0';
                    break;
                }
                p++;
            }
            if (!(fp = fopen(buf, "r"))) {
                do_debug(LOG_ERR, "Unable to open configuration file: %s Error msg: %s\n", buf, strerror(errno));
                continue;
            }
            memset(config_input_line, '\0', LEN_1024);
            while (fgets(config_input_line, LEN_1024, fp)) {
                process_input_line(config_input_line, LEN_1024, buf);
            }
            if (fclose(fp) < 0) {
                do_debug(LOG_FATAL, "fclose error:%s", strerror(errno));
            }
        }
        if (pclose(stream) == -1)
            do_debug(LOG_WARN, "pclose error\n");
    }
}

/* get nagios alert threshold value */
void
get_threshold()
{
    /* set nagios value */
    char   *token = strtok(NULL, W_SPACE);
    char    tmp[4][LEN_32];

    if (conf.mod_num >= MAX_MOD_NUM) {
        do_debug(LOG_FATAL, "Too many mod threshold\n");
    }
    sscanf(token, "%[^;];%[.N0-9];%[.N0-9];%[.N0-9];%[.N0-9];", 
           conf.check_name[conf.mod_num], tmp[0], tmp[1], tmp[2], tmp[3]);

    if (!strcmp(tmp[0], "N")) {
        conf.wmin[conf.mod_num] = 0;

    } else {
        conf.wmin[conf.mod_num] = atof(tmp[0]);
    }
    if (!strcmp(tmp[1], "N")) {
        conf.wmax[conf.mod_num] = 0;

    } else {
        conf.wmax[conf.mod_num] = atof(tmp[1]);
    }
    if (!strcmp(tmp[2], "N")) {
        conf.cmin[conf.mod_num] = 0;

    } else {
        conf.cmin[conf.mod_num]=atof(tmp[2]);
    }
    if (!strcmp(tmp[3], "N")) {
        conf.cmax[conf.mod_num] = 0;

    } else {
        conf.cmax[conf.mod_num] = atof(tmp[3]);
    }
    conf.mod_num++;
}

void
set_special_field(const char *s)
{
    int    i = 0, j = 0;
    struct module *mod = NULL;

    for (i = 0; i < statis.total_mod_num; i++)
    {
        mod = &mods[i];
        struct mod_info *info = mod->info;
        for (j=0; j < mod->n_col; j++) {
            char *p = info[j].hdr;
            while (*p  == ' ') {
                p++;
            }
            if (strstr(s, p)) {
                info[j].summary_bit = SPEC_BIT;
                mod->spec = 1;
            }
        }
    }
}

void
set_special_item(const char *s)
{
    int    i = 0;
    struct module *mod = NULL;

    for (i = 0; i < statis.total_mod_num; i++)
    {
        mod = &mods[i];
        strcpy(mod->print_item, s);
    }
}

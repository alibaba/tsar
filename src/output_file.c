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

void output_file()
{
	struct	module *mod;
	FILE	*fp = NULL;
	int	i, ret = 0;
	char	line[LEN_10240] = {0};
	char	detail[LEN_4096] = {0};
	char	s_time[LEN_256] = {0};

	if (!(fp = fopen(conf.output_file_path, "a+"))) {
		if (!(fp = fopen(conf.output_file_path, "w")))
			do_debug(LOG_FATAL, "output_file: can't create data file = %s  err=%d", conf.output_file_path, errno);
	}

	sprintf(s_time, "%ld", statis.cur_time);
	strcat(line, s_time);

	for (i = 0; i < statis.total_mod_num; i++) {
		mod = &mods[i];
		if (mod->enable && strlen(mod->record)) {
			/* save collect data to output_file */
			sprintf(detail, "%s%s%s%s", SECTION_SPLIT, mod->opt_line, STRING_SPLIT, mod->record);
			strcat(line, detail);
			ret = 1;
		}
	}
	strcat(line, "\n");

	if (ret) {
		fputs(line, fp);
		fclose(fp);
	}
        fclose(fp);
        if(chmod(conf.output_file_path, 0666) < 0 )
                do_debug(LOG_WARN, "chmod file %s error\n",conf.output_file_path);
}


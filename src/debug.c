
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


void do_debug(log_level_t level, const char *fmt, ...)
{
	/* FIXME */
	if (level >= conf.debug_level) {
		va_list	argp;
		time_t	timep;

		time(&timep);
		va_start(argp, fmt);
		vfprintf(stderr, fmt, argp);
		fflush(stderr);
		va_end(argp);
	}

	if (level == LOG_FATAL)
		exit(1);
}

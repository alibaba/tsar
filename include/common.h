
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


#ifndef TSAR_COMMON_H
#define TSAR_COMMON_H


#define PRE_RECORD_FILE         "/tmp/.tsar.tmp"

/*
 * convert data to array
 */
int convert_record_to_array(U_64 *array, int l_array, const char *record);
void get_mod_hdr(char hdr[], const struct module *mod);
int strtok_next_item(char item[], char *record, int *start);
int merge_mult_item_to_array(U_64 *array, struct module *mod);
int get_strtok_num(const char *str, const char *split);
int get_st_array_from_file(int have_collect);


#endif

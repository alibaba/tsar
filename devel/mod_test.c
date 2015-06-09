
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

#define STATS_TEST_SIZE (sizeof(struct stats_test))

static const char *test_usage = "    --test               test information";

/*
 * temp structure for collection infomation.
 */
struct stats_test {
    unsigned long long    value_1;
    unsigned long long    value_2;
    unsigned long long    value_3;
};

/* Structure for tsar */
static struct mod_info test_info[] = {
    {"value1", SUMMARY_BIT,  0,  STATS_NULL},
    {"value2", DETAIL_BIT,  0,  STATS_NULL},
    {"value3", DETAIL_BIT,  0,  STATS_NULL}
};

static void
read_test_stats(struct module *mod, const char *parameter)
{
    /* parameter actually equals to mod->parameter */
    char               buf[256];
    struct stats_test  st_test;

    memset(buf, 0, sizeof(buf));
    memset(&st_test, 0, sizeof(struct stats_test));

    st_test.value_1 = 1;
    st_test.value_2 = 1;
    st_test.value_3 = 1;

    int pos = sprintf(buf, "%llu,%llu,%llu",
            /* the store order is not same as read procedure */
            st_test.value_1,
            st_test.value_2,
            st_test.value_3);

    buf[pos] = '\0';
    /* send data to tsar you can get it by pre_array&cur_array at set_test_record */
    set_mod_record(mod, buf);
    return;
}

static void
set_test_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    /* set st record */
    for (i = 0; i < mod->n_col; i++) {
        st_array[i] = cur_array[i];
    }
}

/* register mod to tsar */
void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--test", test_usage, test_info, 3, read_test_stats, set_test_record);
}

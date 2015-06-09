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

/*
 * Structure for rpi infomation.
 */
struct stats_rpi {
    int cpu_temp;
};

#define STATS_TEST_SIZE (sizeof(struct stats_rpi))

static char *rpi_usage = "    --rpi               Rapsberry Pi information (CPU temprature ...)";


static void
read_rpi_stats(struct module *mod, char *parameter)
{
    FILE   *fp;
    char    buf[64];
    struct  stats_rpi st_rpi;

    memset(buf, 0, sizeof(buf));
    memset(&st_rpi, 0, sizeof(struct stats_rpi));

    if ((fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) == NULL) {
        return;
    }

    int    cpu_temp;

    if (fscanf(fp, "%d", &cpu_temp) != 1) {
        fclose(fp);
        return;
    }

    if (cpu_temp == 85 * 1000 || cpu_temp < 1) {
        fclose(fp);
        return;
    }

    st_rpi.cpu_temp = cpu_temp;

    int pos = sprintf(buf, "%u",
            /* the store order is not same as read procedure */
            st_rpi.cpu_temp);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
    fclose(fp);
    return;
}

static struct mod_info rpi_info[] = {
    {"  temp", SUMMARY_BIT,  0,  STATS_NULL}
};

static void
set_rpi_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    st_array[0] = cur_array[0] / 1000.0;
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--rpi", rpi_usage, rpi_info, 1, read_rpi_stats, set_rpi_record);
}

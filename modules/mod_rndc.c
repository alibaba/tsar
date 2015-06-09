/* rndc: Remote Name Daemon Controller
   daoxian, 2011-11-8
   Copyright(C) Taobao Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tsar.h"


static char *rndc_usage = "    --rndc               information for rndc stats";
static char g_buf[LEN_4096];

static void
create_script()
{
    const char *script = ""
        "#!/usr/bin/perl\n"
        "use strict;\n"
        "\n"
        "my $stat_file_name = get_stat_file_name('/usr/local/pharos/conf/named.conf');\n"
        "# my @label_arr = ('++ Socket I/O Statistics ++', '++ Incoming Requests ++', '++ Incoming Queries ++', '++ Name Server Statistics ++');\n"
        "my @label_arr = ('++ Incoming Requests ++', '++ Name Server Statistics ++');\n"
        "# my %display_key_hash = ('QUERY' => 'query', 'A' => 'address', 'CNAME' => 'cname', 'NS' => 'ns', 'SOA' => 'soa');\n"
        "my %display_key_hash = ('QUERY' => 'qps', 'response time 0ms to 5ms' => 'rt_05', 'response time 5ms to 10ms' => 'rt_10', 'response time 10ms to 20ms' => 'rt_20', 'response time 20ms to 50ms' => 'rt_50',  'response time 50ms to Xms' => 'rt_50+');\n"
        "my %result = ();\n"
        "\n"
        "foreach my $label (@label_arr)\n"
        "{\n"
        "   my @stat_arr = get_stat_section($stat_file_name, $label);\n"
        "   foreach my $stat_result (@stat_arr)\n"
        "   {\n"
        "       $stat_result =~ m|^\\s*(\\d+)\\s+(.+?)\\s*$|;\n"
        "       my $display_key = get_display_key($2);\n"
        "       $result{$display_key} = $1 if $display_key;\n"
        "   }\n"
        "}\n"
        "foreach my $k (keys %display_key_hash)\n"
        "{\n"
        "   $result{$display_key_hash{$k}} = 0 if not defined($result{$display_key_hash{$k}});\n"
        "}\n"
        "\n"
        "foreach my $k (sort keys %result)\n"
        "{\n"
        "   my $v = $result{$k};\n"
        "   print \"$k,$v\\n\";\n"
        "}\n"
        "\n"
        "sub get_display_key\n"
        "{\n"
        "   my $key = shift;\n"
        "   if ($display_key_hash{$key})\n"
        "   {\n"
        "       return $display_key_hash{$key};\n"
        "   }\n"
        "   else\n"
        "   {\n"
        "       # my $ret = '';\n"
        "       # map { $ret .= lc(substr($_, 0, 1)); } split /\\s+/, $key;\n"
        "       # return $ret;\n"
        "       return '';\n"
        "   }\n"
        "}\n"
        "\n"
        "\n"
        "sub get_stat_file_name\n"
        "{\n"
        "   my $conf_file = shift;\n"
        "   open FH, $conf_file or die \"error open file: $conf_file\\n\";\n"
        "   my @arr = <FH>;\n"
        "   close FH;\n"
        "   @arr = grep /\\bstatistics-file\\b/, @arr;\n"
        "   $arr[0] =~ m|\\s*statistics-file\\s+[\"'](.+)[\"']\\s*;|;\n"
        "   return $1 ? $1 : '';\n"
        "}\n"
        "\n"
        "\n"
        "sub get_stat_section\n"
        "{\n"
        "   my $stat_file = shift;\n"
        "   my $seperator = shift;\n"
        "\n"
        "   open FH, $stat_file or die \"error open file: $stat_file\\n\";\n"
        "   my @stat_arr = <FH>;\n"
        "   close FH;\n"
        "   my $stat_str = join \"\", @stat_arr;\n"
        "   $stat_str =~ s/\\r/\\n/g;\n"
        "   $stat_str =~ m/\\Q$seperator\\E\\n+((.|\\n)*?)\\n+\\+\\+/;\n"
        "   my $val = $1;\n"
        "   $val =~ s/\\n\\s+/\\n/g;\n"
        "   $val =~ s/^\\s+//g;\n"
        "   return split /\\n/, $val;\n"
        "}\n";

    FILE *fp = fopen("/tmp/rndc_tsar.pl", "w");
    if (!fp) {
        return;
    }
    fputs(script, fp);
    fclose(fp);
}

static void
exec_script()
{
    if (system("/usr/local/pharos/sbin/rndc -c /usr/local/pharos/conf/trndc.conf stats") < 0) {
        exit(-1);
    }
    if (system("perl /tmp/rndc_tsar.pl > /tmp/rndc_tsar.txt") < 0) {
        exit(-1);
    }
    if (system("echo -n 'badvs,' >> /tmp/rndc_tsar.txt; MYSQL_BIN=`/bin/rpm -ql mysql|/bin/egrep -e '/bin/mysql$'` && ${MYSQL_BIN} -ss -uroot -Ddns_config -e 'SELECT COUNT(name) FROM vs WHERE in_use=1 AND available=0' >> /tmp/rndc_tsar.txt") < 0) {
        exit(-1);
    }
}

static void
parse_stat_file(char buf[])
{
    FILE *fp = fopen("/tmp/rndc_tsar.txt", "r");
    if (!fp) {
        return;
    }

    rewind(fp);
    int pos = 0;
    char line[LEN_128];
    while (fgets(line, LEN_128, fp)) {
        char *s = strtok(line, ",");
        if (!s) {
            continue;
        }
        s = strtok(NULL, ",");
        if (!s) {
            continue;
        }
        if (strlen(s) > 0) {
            s[strlen(s) - 1] = '\0';
        }
        pos += sprintf(buf + pos, "%s,", s);
    }
    fclose(fp);
    if (system("rm /tmp/rndc_tsar.txt") < 0){
        exit(-1);
    }

    if (pos > 0) {
        buf[pos - 1] = '\0';

    } else {
        buf[pos] = '\0';
    }
}

static void
read_rndc_stats(struct module *mod)
{
    memset(g_buf, 0, sizeof(g_buf));
    create_script();
    exec_script();
    parse_stat_file(g_buf);
    set_mod_record(mod, g_buf);
}

static void
set_rndc_stats(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 6; i ++)
    {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = (cur_array[i] - pre_array[i]) / inter;

        } else {
            st_array[i] = 0;
        }
    }
    st_array[6] = cur_array[i];
}

static struct mod_info rndc_info[] = {
    {"   qps", SUMMARY_BIT, MERGE_NULL,  STATS_NULL},
    {" rt_05", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {" rt_10", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {" rt_20", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {" rt_50", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {"rt_50+", DETAIL_BIT, MERGE_NULL,  STATS_NULL},
    {"badvs",  DETAIL_BIT, MERGE_NULL,  STATS_NULL},
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--rndc", rndc_usage, rndc_info, sizeof(rndc_info) / sizeof(struct mod_info), read_rndc_stats, set_rndc_stats);
}

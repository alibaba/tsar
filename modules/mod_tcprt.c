#include "tsar.h"

/*
*模块用来采集tcprt的实时分析结果，tcprt的分析结果每一分钟改变一次，所以tsar的数据也是每分钟更新一次
*/


#define file_avg_bytes          "/sys/module/tcp_rt_cdn/parameters/avg_bytes"
#define file_avg_bytes81        "/sys/module/tcp_rt_cdn/parameters/avg_bytes81"
#define file_avg_drop           "/sys/module/tcp_rt_cdn/parameters/avg_drop"
#define file_avg_drop81         "/sys/module/tcp_rt_cdn/parameters/avg_drop81"
#define file_avg_rt             "/sys/module/tcp_rt_cdn/parameters/avg_rt"
#define file_avg_rt81           "/sys/module/tcp_rt_cdn/parameters/avg_rt81"
#define file_avg_server_time    "/sys/module/tcp_rt_cdn/parameters/avg_server_time"
#define file_avg_server_time81  "/sys/module/tcp_rt_cdn/parameters/avg_server_time81"
#define file_avg_fail           "/sys/module/tcp_rt_cdn/parameters/avg_fail"


struct stats_tcprt {
    unsigned int avg_bytes;
    unsigned int avg_bytes81;
    unsigned int avg_drop;
    unsigned int avg_drop81;
    unsigned int avg_rt;
    unsigned int avg_rt81;
    unsigned int avg_server_time;
    unsigned int avg_server_time81;
    unsigned int avg_fail;
};

static char *tcprt_usage = "    --tcprt             tcprt stat data,(drop、dorp81、fail(1/1000) etc...)";

static int
get_value(const char *path)
{
    FILE *fp;
    char    line[LEN_4096];
    unsigned int value = 0;

    memset(line, 0, LEN_4096);

    if ((fp = fopen(path, "r")) == NULL) {
        return 0;
    }

    if(fgets(line, LEN_4096, fp) != NULL) {
        if(sscanf(line, "%u", &value) <= 0){
            value = 0;
        }
    }

    fclose(fp);
    return value;
}


static void
read_tcprt_stats(struct module *mod)
{
    char    buf[LEN_4096];
    char    item_value[LEN_256];
    struct  stats_tcprt st_tcprt;

    memset(buf, 0, LEN_4096);
    memset(&st_tcprt, 0, sizeof(struct stats_tcprt));

    st_tcprt.avg_bytes = get_value(file_avg_bytes);
    st_tcprt.avg_bytes81 = get_value(file_avg_bytes81);
    st_tcprt.avg_rt = get_value(file_avg_rt);
    st_tcprt.avg_rt81 = get_value(file_avg_rt81);
    st_tcprt.avg_drop = get_value(file_avg_drop);
    st_tcprt.avg_drop81 = get_value(file_avg_drop81);
    st_tcprt.avg_server_time = get_value(file_avg_server_time);
    st_tcprt.avg_server_time81 = get_value(file_avg_server_time81);
    st_tcprt.avg_fail = get_value(file_avg_fail);

    int pos = sprintf(buf, "%u,%u,%u,%u,%u,%u,%u,%u,%u",
            st_tcprt.avg_bytes,
            st_tcprt.avg_bytes81,
            st_tcprt.avg_drop,
            st_tcprt.avg_drop81,
            st_tcprt.avg_rt,
            st_tcprt.avg_rt81,
            st_tcprt.avg_server_time,
            st_tcprt.avg_server_time81,
            st_tcprt.avg_fail);

    buf[pos] = '\0';
    set_mod_record(mod, buf);
}


static struct mod_info tcprt_info[] = {
    {"  objs", DETAIL_BIT,  0,  STATS_NULL}, /*80端口平均对象大小*/
    {"objs81", DETAIL_BIT,  0,  STATS_NULL}, /*81端口平均对象大小*/
    {"  drop", DETAIL_BIT,  0,  STATS_NULL}, /*80端口丢包率(千分之)*/
    {"drop81", DETAIL_BIT,  0,  STATS_NULL}, /*81端口丢包率(千分之)*/
    {"    rt", DETAIL_BIT,  0,  STATS_NULL}, /*80端口平均网络时间*/
    {"  rt81", DETAIL_BIT,  0,  STATS_NULL}, /*81端口平均网络时间*/
    {"   fbt", DETAIL_BIT,  0,  STATS_NULL}, /*80端口平均首字节时间*/
    {" fbt81", DETAIL_BIT,  0,  STATS_NULL}, /*81端口平均首字节时间*/
    {"  fail", DETAIL_BIT,  0,  STATS_NULL}, /*请求没有传输完的比例(千分之)*/
};


static void
set_tcprt_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{

    int i;
    /* set st record */
    for (i = 0; i < 9; i++) {
        st_array[i] = cur_array[i];
    }


    /*所有百分比的单位都是千分之*/
    st_array[3] /= 100.0;
}



    void
mod_register(struct module *mod)
{
    register_mod_fileds(mod, "--tcprt", tcprt_usage, tcprt_info, 9, read_tcprt_stats, set_tcprt_record);
}

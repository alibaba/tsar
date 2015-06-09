#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include "tsar.h"

#define LEN_256     256
#define TRUE        1
#define FALSE       0

#define DATA_PATH   "/tmp/tcprt.data"

//1378694300 all port:80 rt:128 servertime:16 drop:19 rtt:36 fail:1 outbytes:18613 requestnummber:44925
struct stats_tcprt {
    unsigned int avg_port;
    unsigned int avg_rt;
    unsigned int avg_server_time;
    unsigned int avg_drop;
    unsigned int avg_rtt;
    unsigned int avg_fail;
    unsigned int avg_outbytes;
    unsigned int avg_request_num;
};

static char *tcprt_usage = "    --tcprt             tcprt stat data,(bytes、rt、drop etc...)";


void prepare_data(char* shell_string)
{
    FILE   *stream;
    char    buf[LEN_256];

    stream = popen( shell_string, "r" );
    int     b_data_open = FALSE;
    int     fd = 0;
    while(fgets(buf,LEN_256,stream)){
        if(!b_data_open){
            remove(DATA_PATH);
            fd = open(DATA_PATH, O_WRONLY|O_CREAT, 0666);
            if(fd < 0){
                printf("open file %s failed. maybe access authority, try to delete it and exec again\n",DATA_PATH);
                pclose( stream );
                return;
            }

            b_data_open = TRUE;
        }

        write(fd,buf,strlen(buf));
    }
    pclose( stream );
    if (b_data_open && fd > 0) {
        close(fd);
    }
    return;
}



static void
read_tcprt_stats(struct module *mod)
{
    int     pos = 0;
    FILE   *fp = NULL;
    char    buf[LEN_4096];
    char    line[LEN_256];
    struct  stats_tcprt st_tcprt;

    memset(buf, 0, LEN_4096);
    memset(&st_tcprt, 0, sizeof(struct stats_tcprt));

    prepare_data("cat /sys/kernel/debug/rt-network-real* | sort -nrk 1\
                  | awk 'BEGIN{tmp=0}{if(NR==1 || (tmp > 0 && tmp==$1)){tmp = $1;print $3,$4,$5,$6,$7,$8,$9,$10}}END{}'");


    fp=fopen(DATA_PATH, "r");
    if(!fp){
        printf("open file %s failed. maybe access authority, try to delete it and exec again\n",DATA_PATH);
        return;
    }

    while(fgets(line,sizeof(line) - 1,fp)){
        if(sscanf(line,
                    "%u %u %u %u %u %u %u %u",
                    &st_tcprt.avg_port,
                    &st_tcprt.avg_rt,
                    &st_tcprt.avg_server_time,
                    &st_tcprt.avg_drop,
                    &st_tcprt.avg_rtt,
                    &st_tcprt.avg_fail,
                    &st_tcprt.avg_outbytes,
                    &st_tcprt.avg_request_num) > 0 )
        {

            pos += sprintf(buf + pos, "port%u=%u,%u,%u,%u,%u,%u,%u",
                    st_tcprt.avg_port,
                    st_tcprt.avg_rt,
                    st_tcprt.avg_server_time,
                    st_tcprt.avg_drop,
                    st_tcprt.avg_rtt,
                    st_tcprt.avg_fail,
                    st_tcprt.avg_outbytes,
                    st_tcprt.avg_request_num);
            pos += sprintf(buf + pos, ITEM_SPLIT);
        }
    }

    if (pos == 0) {
        fclose(fp);
        return;
    }

    fclose(fp);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}



static struct mod_info tcprt_info[] = {
    {"    rt", DETAIL_BIT,  0,  STATS_NULL}, /*平均网络时间*/
    {"   fbt", DETAIL_BIT,  0,  STATS_NULL}, /*平均首字节时间*/
    {"  drop", DETAIL_BIT,  0,  STATS_NULL}, /*丢包率*/
    {"   rtt", DETAIL_BIT,  0,  STATS_NULL}, /*rtt*/
    {"  fail", DETAIL_BIT,  0,  STATS_NULL}, /*请求没有传输完的比例*/
    {"  objs", DETAIL_BIT,  0,  STATS_NULL}, /*平均对象大小*/
    {"  reqs", DETAIL_BIT,  0,  STATS_NULL}, /*平均对象大小*/
};


static void
set_tcprt_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{

    int i;
    /* set st record */
    for (i = 0; i < 7; i++) {
        st_array[i] = cur_array[i];
    }
}



    void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--tcprt", tcprt_usage, tcprt_info, 7, read_tcprt_stats, set_tcprt_record);
}

#include<stdio.h>
#include<stdint.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include "tsar.h"

#define LEN_256 256
#define DATA_PATH "/tmp/live_tcprt.data"
#define TRUE  1
#define FALSE 0
struct stats_live_tcprt{
	unsigned int 	 connections;       //建立连接总数
	unsigned int     i_sum_out_bytes;   //传输时的发送字节数
	unsigned int     i_sum_drop_packets;//传输时的丢包数
	unsigned int     i_avg_rtt;  		//平均传输时的RTT
	unsigned int     e_sum_out_bytes;   //连接结束时的发送字节数
	unsigned int     e_sum_drop_packets;//连接结束时的丢包数
	unsigned int     e_avg_rtt;			//平均连接结束时的RTT
	unsigned int     i_records;			//所有的传输次数
	unsigned int     e_records;			//所有的连接结束次数
};
static char* live_tcprt_usage ="    --live-tcprt	live-tcprt stats average data(outbytes、rt、drop etc...)";

static struct mod_info live_tcprt_info[] = {
	    {"  Conn", DETAIL_BIT,  0,  STATS_NULL}, /*建立连接总数*/
		{"Ioutbt", DETAIL_BIT,  0,  STATS_NULL}, /*传输时的发送字节数*/
		{"Idppak", DETAIL_BIT,  0,  STATS_NULL}, /*传输时的丢包数*/
		{"  Irtt", DETAIL_BIT,  0,  STATS_NULL}, /*平均传输时的RTT*/
		{"Eoutbt", DETAIL_BIT,  0,  STATS_NULL}, /*连接结束时的发送字节数*/
		{"Edppak", DETAIL_BIT,  0,  STATS_NULL}, /*连接结束时的丢包数*/
		{"  Ertt", DETAIL_BIT,  0,  STATS_NULL}, /*平均连接结束时的RTT*/
};

void prepare_data(char* shell_string){
	FILE*       stream;
	char  buf[LEN_256];
	stream = popen(shell_string, "r");
	int fd=0;
	int is_datafile_open=FALSE;
	while(fgets(buf,LEN_256,stream)){
		if(!is_datafile_open){
			remove(DATA_PATH);
			fd = open(DATA_PATH, O_WRONLY|O_CREAT ,0666);
			if(fd < 0){
				printf("open file %s failed. maybe access authority, try to delete it and exec again\n",DATA_PATH);
				pclose(stream);
				return;
			}
			is_datafile_open = TRUE;
		}
		write(fd,buf,strlen(buf));
	}
	pclose(stream);
	if(is_datafile_open && fd >0){
		close(fd);
	}
	return;
}

static void
read_live_tcprt_stats(struct module* mod){
	
	char   buf[LEN_4096];
	int             pos=0;
	FILE*         fd=NULL;
	char    line[LEN_256];
	struct  stats_live_tcprt sl_tcprt;
	int row             =1;
	unsigned int counter=0;
	unsigned int para5=0, para6=0, para7=0;

	memset(buf, 0, LEN_4096);
	memset(line,0, LEN_256);
	memset(&sl_tcprt, 0, sizeof(struct stats_live_tcprt));
	
	// /sys/kernel/debug/tcp-watch-log*
	prepare_data("cat /sys/kernel/debug/tcp-watch-log* | sort -nrk 1 | awk 'BEGIN{tmps=\"S\";tmpi=\"I\";tmpe=\"E\";\
			             		       sums=0;sumi=0;sume=0;sp5=0;sp6=0;sp7=0;ip5=0;ip6=0;ip7=0;ep5=0;ep6=0;ep7=0;}\
						                      {if(tmps==$1){sums+=1;}if(tmpi==$1){sumi+=1;ip5+=$5;ip6+=$6;ip7+=$7;}\
											                        if(tmpe==$1){sume+=1;ep5+=$5;ep6+=$6;ep7+=$7;}}\
								      END{print sums,sp5,sp6,sp7; print sumi,ip5,ip6,ip7; print sume,ep5,ep6,ep7}'");

	fd =fopen(DATA_PATH,"r");
	if(!fd){
		printf("open file %s failed. maybe access authority, try to delete it and exec again\n",DATA_PATH);	
		return;
	}
	while(fgets(line, sizeof(line)-1,fd )) {
		if(sscanf(line,
					"%u %u %u %u",
					&counter,
					&para5,
					&para6,
					&para7)>0)
		{
			switch(row){
				case 1:
					sl_tcprt.connections       = counter;
					break;
				case 2:
					sl_tcprt.i_records         = counter;
					sl_tcprt.i_sum_out_bytes   = para5;
					sl_tcprt.i_sum_drop_packets= para6;
					sl_tcprt.i_avg_rtt         = para7;
					break;
				case 3:
					sl_tcprt.e_records         = counter;
					sl_tcprt.e_sum_out_bytes   = para5;
					sl_tcprt.e_sum_drop_packets= para6;
					sl_tcprt.e_avg_rtt         = para7;
					break;
				default:
					break;
			}
			row++;
		}
	}
	if(sl_tcprt.i_records > 0){
		//sl_tcprt.i_sum_out_bytes   /=sl_tcprt.i_records;
		//sl_tcprt.i_sum_drop_packets/=sl_tcprt.i_records;
		sl_tcprt.i_avg_rtt         /=sl_tcprt.i_records;
	}
	if(sl_tcprt.e_records > 0){
		//sl_tcprt.e_sum_out_bytes   /=sl_tcprt.e_records;
		//sl_tcprt.e_sum_drop_packets/=sl_tcprt.e_records;
		sl_tcprt.e_avg_rtt		   /=sl_tcprt.e_records;
	}

	if(row!=4){
		fclose(fd);
		return;
	}
	pos +=sprintf(buf+pos,"%u,%u,%u,%u,%u,%u,%u",
						  sl_tcprt.connections,
						  sl_tcprt.i_sum_out_bytes,
						  sl_tcprt.i_sum_drop_packets,
						  sl_tcprt.i_avg_rtt,
						  sl_tcprt.e_sum_out_bytes,
						  sl_tcprt.e_sum_drop_packets,
						  sl_tcprt.e_avg_rtt);
	buf[pos]='\0';
	fclose(fd);
	set_mod_record(mod,buf);
    
}

static void
set_live_tcprt_stats(struct module *mod, double st_array[],
		    U_64 pre_array[], U_64 cur_array[], int inter){
	int i;
	for(i=0;i<7;i++){
		st_array[i] = cur_array[i];
	}
}

void 
mod_register(struct module* mod){
	register_mod_fields(mod, "--live-tcprt", live_tcprt_usage, live_tcprt_info, 7, read_live_tcprt_stats, 
			set_live_tcprt_stats);
}

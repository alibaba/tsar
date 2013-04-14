CFLAGS = -Wall -fPIC --shared -g -O2
CC = gcc
INCLUDE_DIR = ../include
LINK = $(CC) -I$(INCLUDE_DIR) $(CFLAGS)


OBJS =  mod_swap.so mod_partition.so mod_cpu.so mod_mem.so mod_lvs.so mod_haproxy.so \
	mod_traffic.so mod_squid.so mod_load.so mod_tcp.so mod_udp.so mod_tcpx.so \
	mod_apache.so mod_pcsw.so mod_io.so mod_nginx.so mod_cgblkio.so \
	mod_cgcpu.so mod_cgmem.so mod_ncpu.so mod_rndc.so \
	mod_ts_cache.so mod_ts_client.so mod_ts_err.so mod_ts_os.so mod_ts_storage.so mod_ts_conn.so mod_ts_codes.so\
	mod_swift.so mod_swift_code.so mod_swift_store.so mod_swift_fwd.so \
    mod_swift_tcmalloc.so mod_tmd.so mod_percpu.so mod_rpi.so

all: $(OBJS)

$(OBJS): %.so: %.c
	$(LINK) $< -o $@
clean:
	rm -f *.so;

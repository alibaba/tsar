CFLAGS = -Wall -fPIC --shared -g -O2 -Wno-strict-aliasing
CC = gcc
INCLUDE_DIR = ../include
LINK = $(CC) -I$(INCLUDE_DIR) $(CFLAGS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LINK += -Wl,-undefined -Wl,dynamic_lookup

    OBJS =  mod_swap.so mod_cpu.so mod_mem.so mod_lvs.so mod_haproxy.so \
            mod_traffic.so mod_pernic.so mod_squid.so mod_load.so mod_tcp.so mod_udp.so mod_tcpx.so mod_proc.so\
            mod_apache.so mod_pcsw.so mod_nginx.so mod_nginx_code.so mod_cgblkio.so \
            mod_cgcpu.so mod_cgmem.so mod_ncpu.so mod_rndc.so \
            mod_ts_cache.so mod_ts_client.so mod_ts_err.so mod_ts_os.so \
            mod_ts_storage.so mod_ts_conn.so mod_ts_codes.so\
            mod_swift.so mod_swift_code.so mod_swift_store.so mod_swift_spdy.so\
            mod_swift_fwd.so mod_swift_domain.so mod_swift_swapdir.so \
            mod_swift_purge.so mod_swift_tcmalloc.so mod_swift_sys.so \
            mod_swift_esi.so mod_swift_balancer.so mod_swift_blc_fwd.so mod_swift_conn.so\
            mod_tmd.so mod_percpu.so mod_rpi.so mod_tcprt.so mod_shell.so mod_nginx_domain.so mod_pharos.so mod_pharos_load.so
else
    OBJS =  mod_swap.so mod_partition.so mod_cpu.so mod_mem.so mod_lvs.so mod_haproxy.so \
            mod_traffic.so mod_pernic.so mod_squid.so mod_load.so mod_tcp.so mod_udp.so mod_tcpx.so mod_proc.so\
            mod_apache.so mod_pcsw.so mod_io.so mod_nginx.so mod_nginx_code.so mod_cgblkio.so \
            mod_cgcpu.so mod_cgmem.so mod_ncpu.so mod_rndc.so \
            mod_ts_cache.so mod_ts_client.so mod_ts_err.so mod_ts_os.so \
            mod_ts_storage.so mod_ts_conn.so mod_ts_codes.so\
            mod_swift.so mod_swift_code.so mod_swift_store.so mod_swift_spdy.so\
            mod_swift_fwd.so mod_swift_domain.so mod_swift_swapdir.so \
            mod_swift_purge.so mod_swift_tcmalloc.so mod_swift_sys.so \
            mod_swift_esi.so mod_swift_balancer.so mod_swift_blc_fwd.so mod_swift_conn.so\
            mod_tmd.so mod_percpu.so mod_rpi.so mod_tcprt.so mod_shell.so mod_nginx_domain.so mod_pharos.so mod_pharos_load.so
endif

all: $(OBJS)

$(OBJS): %.so: %.c
	$(LINK) $< -o $@
clean:
	rm -f *.so;

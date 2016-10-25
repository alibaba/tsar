DIRS = src modules lualib

all:
	for i in $(DIRS); do make -C $$i; done

clean:
	for i in $(DIRS); do cd $$i;make clean;cd ..; done

install: all
	#mkdir for tsar
	mkdir -p /usr/local/tsar/modules
	mkdir -p /etc/tsar
	mkdir -p /usr/local/man/man8/
	#copy tsar shared so
	cp modules/*.so /usr/local/tsar/modules
	#copy bin file
	cp src/tsar /usr/bin/tsar
	#copy config file
	cp conf/tsar.conf /etc/tsar/tsar.conf
	cp conf/tsar.logrotate /etc/logrotate.d/tsar
	cp conf/tsar.cron /etc/cron.d/tsar
	#copy man file
	cp conf/tsar.8 /usr/local/man/man8/
	#install lualib
	make -C lualib install	

tsardevel:
	mkdir -p /usr/local/tsar/devel
	cp devel/mod_test.c /usr/local/tsar/devel/mod_test.c
	cp devel/mod_test.conf /usr/local/tsar/devel/mod_test.conf
	cp devel/tsar.h /usr/local/tsar/devel/tsar.h
	cp devel/Makefile.test /usr/local/tsar/devel/Makefile.test
	cp devel/tsardevel /usr/bin/tsardevel


	ctags -R
	cscope -Rbq

tsarluadevel:
	mkdir -p /usr/local/tsar/luadevel
	cp luadevel/mod_lua_test.lua /usr/local/tsar/luadevel/mod_lua_test.lua
	cp luadevel/mod_lua_test.conf /usr/local/tsar/luadevel/mod_lua_test.conf
	cp luadevel/Makefile.test /usr/local/tsar/luadevel/Makefile.test
	cp luadevel/tsarluadevel /usr/bin/tsarluadevel

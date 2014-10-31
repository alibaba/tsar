DIRS = modules src

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

tsardevel:
	mkdir -p /usr/local/tsar/devel
	cp devel/mod_test.c /usr/local/tsar/devel/mod_test.c
	cp devel/mod_test.conf /usr/local/tsar/devel/mod_test.conf
	cp devel/tsar.h /usr/local/tsar/devel/tsar.h
	cp devel/Makefile.test /usr/local/tsar/devel/Makefile.test
	cp devel/tsardevel /usr/bin/tsardevel

uninstall:
	#rm tsar
	rm -rf /usr/local/tsar
	rm -rf /etc/tsar/cron.d
	rm -f /etc/logrotate.d/tsar
	rm -f /etc/cron.d/tsar
	rm -f /usr/local/man/man8/tsar.8
	#rm tsar
	rm -f /usr/bin/tsar
	#rm tsardevel
	rm -f /usr/bin/tsardevel
	#backup configure file
	if [ -f /etc/tsar/tsar.conf ]; then mv /etc/tsar/tsar.conf /etc/tsar/tsar.conf.rpmsave; fi
	#backup the log data file
	if [ -f /var/log/tsar.data ]; then mv /var/log/tsar.data /var/log/tsar.data.bak; fi

tags:
	ctags -R
	cscope -Rbq

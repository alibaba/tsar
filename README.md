Introduction
------------
Tsar(Taobao System Activity Reporter) is an system and application monitor tools, such as system info(cpu, load, io), or apps info(nginx, haproxy). The collect data can be stored at local disk, you can also send the data to nagios.
It is very convenient to add custom modules for tsar, you just need to write collect function and report function as requested.

Installation
-------------
Tsar is now available on github, you can clone it and install as follows:

    $git clone git://github.com/kongjian/tsar.git
    $cd tsar
    $make
    $make install
or you can just download zip file and install it an follows:

    $wget -O tsar.zip https://github.com/alibaba/tsar/archive/master.zip
    $unzip tsar.zip
    $cd tsar
    $make
    $make install

after install, some major file is:

* Tsar configure:`/etc/tsar/tsar.conf`, tsar main configure file;
* cron configure:`/etc/cron.d/tsar`, run tsar collect every minute;
* logrotate configure:`/etc/logrotate.d/tsar` rotate log file tsar.data every month;
* modules path:`/usr/local/tsar/modules`contains all modules dynamic library;

Tsar configure
-------------
after install, it does not have any data,to check tsar, run `tsar -l`, see if real-time collection is normal:

    [kongjian@v132172.sqa.cm4 tsar]$ tsar -l -i 1
    Time              ---cpu-- ---mem-- ---tcp-- -----traffic---- --xvda-- -xvda1-- -xvda2-- -xvda3-- -xvda4-- -xvda5--  ---load-
    Time                util     util   retran    pktin  pktout     util     util     util     util     util     util     load1
    11/04/13-14:09:10   0.20    11.57     0.00     9.00    2.00     0.00     0.00     0.00     0.00     0.00     0.00      0.00
    11/04/13-14:09:11   0.20    11.57     0.00     4.00    2.00     0.00     0.00     0.00     0.00     0.00     0.00      0.00

Tsar main config is `/etc/tsar/tsar.conf`, Often used are;
* add a module, add `mod_<yourmodname> on` to config
* enable or disable a module, use `mod_<yourmodname> on/off`
* parameter for a module, use `mod_<yourmodname> on parameter`
* `output_stdio_mod` set which modules will be output to stdio when use `tsar`
* `output_file_path` file to store history data, (you should modify logrotate config `/etc/logrotate.d/tsar` corresponding it)
* `output_interface` specify tsar data output destination. default is local file, nagios/db is to remote, see advanced usage for nagios/db.

Tsar usage
-------------
* see history :`tsar`
* -l/--list :list available moudule
* -l/--live :show real-time info, `tsar -l --cpu`
* -i/--interval :set interval for report, `tsar -i 1 --cpu`
* --modname :specify module to show, `tsar --cpu`
* -s/--spec :specify module detail field, `tsar --cpu -s sys,util`
* -d/--date :specify data, YYYYMMDD, or n means n days ago
* -C/--check :show the last collect data
* -d/--detail :show the module all fields information
* -h/--help :show help

Advanced usage
-------------
* output to nagios

config：

add output type `output_interface file,nagios` at tsar main config

configure nagios server address, port, and send interval time

    ####The IP address or the host running the NSCA daemon
    server_addr nagios.server.com
    ####The port on which the daemon is running - default is 5667
    server_port 8086
    ####The cycle of send alert to nagios
    cycle_time 300

as tsar use nagios passive mode, it need nsca bin and config location

    ####nsca client program
    send_nsca_cmd /usr/bin/send_nsca
    send_nsca_conf /home/a/conf/amon/send_nsca.conf

then specify module and field to be checked, there are 4 threshold corresponding to nagios different level

    ####tsar mod alert config file
    ####threshold servicename.key;w-min;w-max;c-min;cmax;
    threshold cpu.util;50;60;70;80;

* output to mysql

config:

add output type `output_interface file,db` at tsar main config

then specify which module will be output:

    output_db_mod mod_cpu,mod_mem,mod_traffic,mod_load,mod_tcp,mod_udpmod_io

configure destination address and port

    output_db_addr console2:56677

destination listen at specific port, it recv data and flush to mysql, you can use tsar2db: https://github.com/kongjian/tsar2db

module develop
-------------
add new module for tsar is a good feature, you can collect your interested data and tsar will handler it for you.

First install tsardevel，`make tsardevel` will do it

run `tsardevel <yourmodname>`, you will have an yourmodname dir and init files.

    [kongjian@v132172.sqa.cm4 tsar]$ tsardevel test
    build:make
    install:make install
    uninstall:make uninstall
    [kongjian@v132172.sqa.cm4 tsar]$ ls test
    Makefile  mod_test.c  mod_test.conf

modify cread_test_stats set_test_record at test.c
after modify, use `make;make install` to install your mod, run `tsar --test` to see your data

More
-------------
The homepage of Tsar is at Taocoded: http://code.taobao.org/p/tsar/

Send any question to kongjian@taobao.com

Introduction
------------
Tsar (Taobao System Activity Reporter) is a monitoring tool, which can be used to gather and summarize system information, e.g. CPU, load, IO, and application information, e.g. nginx, HAProxy, Squid, etc. The results can be stored at local disk or sent to Nagios.

Tsar can be easily extended by writing modules, which makes it a powerful and versatile reporting tool.

Module introduction: [info](https://github.com/alibaba/tsar/blob/master/info.md)

Installation
-------------
Tsar is available on GitHub, you can clone and install it as follows:

    $ git clone git://github.com/kongjian/tsar.git
    $ cd tsar
    $ make
    # make install

Or you can download the zip file and install it:

    $ wget -O tsar.zip https://github.com/alibaba/tsar/archive/master.zip --no-check-certificate
    $ unzip tsar.zip
    $ cd tsar
    $ make
    # make install

After installation, you may see these files:

* `/etc/tsar/tsar.conf`, which is tsar's main configuration file;
* `/etc/cron.d/tsar`, is used to run tsar to collect information every minute;
* `/etc/logrotate.d/tsar` will rotate tsar's log files every month;
* `/usr/local/tsar/modules` is the directory where all module libraries (*.so) are located;

Configuration
-------------
There is no output displayed after installation by default. Just run `tsar -l` to see if the real-time monitoring works, for instance:

    [kongjian@tsar]$ tsar -l -i 1
    Time              ---cpu-- ---mem-- ---tcp-- -----traffic---- --xvda-- -xvda1-- -xvda2-- -xvda3-- -xvda4-- -xvda5--  ---load-
    Time                util     util   retran    pktin  pktout     util     util     util     util     util     util     load1
    11/04/13-14:09:10   0.20    11.57     0.00     9.00    2.00     0.00     0.00     0.00     0.00     0.00     0.00      0.00
    11/04/13-14:09:11   0.20    11.57     0.00     4.00    2.00     0.00     0.00     0.00     0.00     0.00     0.00      0.00

Usually, we configure Tsar by simply editing `/etc/tsar/tsar.conf`:

* To add a module, add a line like `mod_<yourmodname> on`
* To enable or disable a module, use `mod_<yourmodname> on/off`
* To specify parameters for a module, use `mod_<yourmodname> on parameter`
* `output_stdio_mod` is to set modules output to standard I/O
* `output_file_path` is to set history data file, (you should modify the logrotate script `/etc/logrotate.d/tsar` too)
* `output_interface` specifies tsar data output destination, which by default is a local file. See the Advanced section for more information.

Usage
------
* null          :see default mods history data, `tsar`
* --modname     :specify module to show, `tsar --cpu`
* -L/--list     :list available moudule, `tsar -L`
* -l/--live     :show real-time info, `tsar -l --cpu`
* -i/--interval :set interval for report, `tsar -i 1 --cpu`
* -s/--spec     :specify module detail field, `tsar --cpu -s sys,util`
* -D/--detail   :do not conver data to K/M/G, `tsar --mem -D`
* -m/--merge    :merge multiply item to one, `tsar --io -m`
* -I/--item     :show spec item data, `tsar --io -I sda`
* -d/--date     :specify data, YYYYMMDD, or n means n days ago
* -C/--check    :show the last collect data
* -h/--help     :show help, `tsar -h`

Advanced
--------
* Output to Nagios

To turn it on, just set output type `output_interface file,nagios` in the main configuration file.

You should also specify Nagios' IP address, port, and sending interval, e.g.:

    ####The IP address or the hostname running the NSCA daemon
    server_addr nagios.server.com
    ####The port on which the daemon is listening - by default it is 5667
    server_port 8086
    ####The cycle (interval) of sending alerts to Nagios
    cycle_time 300

As tsar uses Nagios' passive mode, so you should specify the nsca binary and its configuration file, e.g.:

    ####nsca client program
    send_nsca_cmd /usr/bin/send_nsca
    send_nsca_conf /home/a/conf/amon/send_nsca.conf

Then specify the module and fields to be checked. There are 4 threshold levels.

    ####tsar mod alert config file
    ####threshold servicename.key;w-min;w-max;c-min;cmax;
    threshold cpu.util;50;60;70;80;

* Output to MySQL

To use this feature, just add output type `output_interface file,db` in tsar's configuration file.

Then specify which module(s) will be enabled:

    output_db_mod mod_cpu,mod_mem,mod_traffic,mod_load,mod_tcp,mod_udpmod_io

Note that you should set the IP address (or hostname) and port where tsar2db listens, e.g.:

    output_db_addr console2:56677

Tsar2db receives sql data and flush it to MySQL. You can find more information about tsar2db at https://github.com/alibaba/tsar2db.


Module development
------------------
Tsar is easily extended. Whenever you want information that is not collected by tsar yet, you can write a module.

First, install the tsardevel tool (`make tsardevel` will do this for you):

Then run `tsardevel <yourmodname>`, and you will get a directory named yourmodname, e.g.:

    [kongjian@tsar]$ tsardevel test
    build:make
    install:make install
    uninstall:make uninstall

    [kongjian@tsar]$ ls test
    Makefile  mod_test.c  mod_test.conf

You can modify the read_test_stats() and set_test_record() functions in test.c as you need.
Then run `make;make install` to install your module and run `tsar --yourmodname` to see the output.

More
----
Homepage http://tsar.taobao.org

Any question, please feel free to contact me by kongjian@taobao.com

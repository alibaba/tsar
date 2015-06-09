#include <sys/sysinfo.h>
#include <linux/param.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "tsar.h"
#include "mod_swift.h"

#define RETRY_NUM 3
#define HOSTNAME "localhost"
#define PORT 82
/* swift's bin file path */
#define SWIFT_FILE_PATH "/usr/sbin/swift"
/* starttime field number(start with 0) in /proc/[pid]/stat */
#define SWIFT_PID_FILE "/var/run/swift.81.pid"
#define START_TIME_FIELD_NO 21

#define SWIFT_FORK_CNT "fork_count"

static const char *usage = "    --swift_sys             Swift System infomation";
int mgrport = 82;

static struct mod_info swift_proc_mod_info[] = {
    {"   pid", DETAIL_BIT,  0,  STATS_NULL}, /* swift pid */
    {"  ppid", DETAIL_BIT,  0,  STATS_NULL}, /* swift's parent pid */
    {"sstart", DETAIL_BIT,  0,  STATS_NULL}, /* swift start time */
    {"pstart", DETAIL_BIT,  0,  STATS_NULL}, /* swift's parent start time */
    {"  core", DETAIL_BIT,  0,  STATS_NULL}, /* swfit's coredump times */
};

/* swift process's info, see swift_proc_info */
struct proc_info_t {
    pid_t pid;
    pid_t ppid;
    unsigned long start_time;
    unsigned long pstart_time;
    unsigned long long coredump_times;
};

/* parent's pid and start time*/
pid_t swift_ppid = 0;
unsigned long swift_pstart_time = 0;
struct proc_info_t swift_proc_info;

/**
 * get system's booting time
 * @retval <0 some error occured.
 */
static time_t get_boot_time()
{
    struct sysinfo info;
    time_t cur_time = 0;

    if (sysinfo(&info))
        return -1;

    time(&cur_time);

    if (cur_time > info.uptime)
        return cur_time - info.uptime;
    else
        return info.uptime - cur_time;
}

/**
 * get process's start time(jiffies) from the string in /proc/pid/stat
 */
static unsigned long parse_proc_start_time(const char *proc_stat)
{
    /* proc_stat has several fields seperated by ' ',
    * now getting the START_TIME_FIELD_NO th field as process' start time
    */
    if (!proc_stat) return 0;
    unsigned long start_time = 0;
    int spaces = 0;
    while (*proc_stat) {
        if (' ' == *proc_stat++) ++spaces;

        if (spaces == START_TIME_FIELD_NO) {
            sscanf(proc_stat, "%lu", &start_time);
            break;
        }
    }

    return start_time;
}

/**
 * check a process whos pid is pid and  absolute path is abs_path
 * is running or not.
 * @retval 1 running
 * @retval 0 not running
 */
static int is_proc_running(pid_t pid, const char *abs_path)
{
    char exe_file[LEN_4096];
    char line[LEN_4096] = {0};
    snprintf(exe_file, LEN_4096 - 1, "/proc/%d/exe", pid);

    if (readlink(exe_file, line, LEN_4096) != 0 &&
        strncmp(line, abs_path, strlen(abs_path)) == 0) {
        return 1;
    }
    return 0;
}
/*
 * read SWIFT_PID_FILE, and get swift's pid, then get the proc's
 * and its parent's infos.
 * if swift is not running, we cannt get its parent's any info.
 */
static void get_proc_info(const char *swift_pid_file, struct proc_info_t *proc_info)
{

    if (!swift_pid_file || !proc_info) return ;
    FILE *fp;
    char stat_file[LEN_4096];
    char line[LEN_4096];
    pid_t pid;
    char dummy_char;
    char dummy_str[LEN_4096];

    /* get pid */
    if ((fp = fopen(swift_pid_file, "r")) == NULL) {
        return ;
    }

    if (fgets(line, LEN_4096, fp) == NULL) {
        fclose(fp);
        return ;
    }
    fclose(fp);

    pid = atoi(line);
    if (pid <= 0) {
        return;
    }
    snprintf(stat_file, LEN_4096 - 1, "/proc/%d/stat", pid);

    /* read stat file */
    if ((fp = fopen(stat_file, "r")) == NULL) {
        return;
    }

    if (fgets(line, LEN_4096, fp) != NULL) {
        fclose(fp);

        sscanf(line, "%d %s %c %d", &proc_info->pid, dummy_str,
            &dummy_char, &proc_info->ppid);
        swift_ppid = proc_info->ppid;
        proc_info->start_time = parse_proc_start_time(line);
    } else {
        fclose(fp);
        return;
    }

    /* read parent's stat file and get start_time */
    snprintf(stat_file, LEN_4096 - 1, "/proc/%d/stat", proc_info->ppid);

    if ((fp = fopen(stat_file, "r")) == NULL) {
        return;
    }

    if (fgets(line, LEN_4096, fp) != NULL) {
        fclose(fp);
        proc_info->pstart_time = parse_proc_start_time(line);
    } else {
        fclose(fp);
        return;
    }

    /* convert jiffies to second */
    proc_info->start_time /= HZ;
    proc_info->pstart_time /= HZ;

    static time_t boot_time = 0;
    if (boot_time == 0) boot_time = get_boot_time();
    proc_info->start_time += boot_time;
    proc_info->pstart_time += boot_time;
    swift_pstart_time = proc_info->pstart_time;
}

static int parse_swift_info(char *buf)
{
    char *line;
    line = strtok(buf, "\n");
    while (line != NULL) {
        read_swift_value(line, SWIFT_FORK_CNT, &swift_proc_info.coredump_times);
        line = strtok(NULL, "\n");
    }
    return 0;
}

static int read_swift_stat(char *cmd, parse_swift_info_func parse_func)
{
    char msg[LEN_1024];
    char buf[1024*1024];
    sprintf(msg,
            "GET cache_object://localhost/%s "
            "HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept:*/*\r\n"
            "Connection: close\r\n\r\n",
            cmd);

    int len, conn = 0, bytesWritten, fsize = 0;

    if (my_swift_net_connect(HOSTNAME, mgrport, &conn, "tcp") != 0) {
        close(conn);
        return -1;
    }

    int flags;

    if ((flags = fcntl(conn, F_GETFL, 0)) < 0) {
        close(conn);
        return -1;
    }

    if (fcntl(conn, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(conn);
        return -1;
    }

    struct timeval timeout = {10, 0};

    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

    bytesWritten = mywrite_swift(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
        close(conn);
        return -2;

    } else if (bytesWritten != strlen(msg)) {
        close(conn);
        return -3;
    }

    while ((len = myread_swift(conn, buf + fsize, sizeof(buf) - fsize)) > 0) {
        fsize += len;
    }

    if (fsize < 100) {
        close(conn);
        return -1;
    }

    if (parse_func && parse_func(buf) < 0) {
        close(conn);
        return -1;
    }

    close(conn);
    return 0;
}

static void read_proc_stat(struct module *mod, char *parameter)
{
    struct proc_info_t *info;
    int    retry = 0;

    info = &swift_proc_info;
    memset(info, 0, sizeof(struct proc_info_t));
    /* parameter is swift's pid file path */
    if (parameter && parameter[0] == '/') {
        get_proc_info(parameter, info);
    } else {
        if (parameter)
            mgrport = atoi(parameter);
        if (!mgrport) {
            mgrport = PORT;
        }
        get_proc_info(SWIFT_PID_FILE, info);
    }
    /* if swift has cored, info.ppid has no value, then we set it mannully */
    if (info->ppid == 0 && is_proc_running(swift_ppid, SWIFT_FILE_PATH)) {
        info->ppid = swift_ppid;
        info->pstart_time = swift_pstart_time;
    }

    while (read_swift_stat("counters", parse_swift_info) < 0 && retry < RETRY_NUM) {
        retry++;
    }

    char buf[LEN_4096];
    memset(buf, 0, sizeof(buf));
    int pos = snprintf(buf, LEN_4096 - 1, "%d,%d,%lu,%lu,%llu",
        info->pid, info->ppid, info->start_time, info->pstart_time, info->coredump_times);
    buf[pos] = '\0';
    set_mod_record(mod, buf);
}

static void set_proc_stat(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 5; ++i) {
        st_array[i] = cur_array[i];
    }
}

void mod_register(struct module *mod)
{
    register_mod_fields(mod, "--swift_sys", usage,
        swift_proc_mod_info, 5, read_proc_stat, set_proc_stat);
}



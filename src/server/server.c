#include "medialib.h"
#include "server_conf.h"
#include "thr_channel.h"
#include "thr_list.h"
#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

struct server_conf_st server_conf = {
    .rcvport = DEFAULT_PORT,
    .mgroup = DEFAULT_IP,
    .media_dir = DEFAULT_MEDIADIR,
    .runmode = RUN_DAEMON,
    .ifname = DEFAULT_IF
};

int sd;
struct sockaddr_in snaddr;
static struct mlib_listentry_st* list;

static void print_help();
static void get_op(int argc, char* argv[]);
static int daemonize();
static void daemon_exit(int s)
{
    thr_channel_destroyall();
    thr_list_destroy();
    mlib_freechnlist(list);

    closelog();
    exit(0);
}
static void socket_init()
{
    struct sockaddr_in laddr;
    struct ip_mreqn mreq;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        syslog(LOG_ERR, "socket(): %s", strerror(errno));
        exit(1);
    }

    inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);

    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0) {
        syslog(LOG_ERR, "setsockopt(): %s", strerror(errno));
        exit(1);
    }

    snaddr.sin_family = AF_INET;
    snaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET, server_conf.mgroup, &snaddr.sin_addr.s_addr);
}

int main(int argc, char* argv[])
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sa.sa_handler = daemon_exit;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    openlog("server", LOG_PID | LOG_PERROR, LOG_DAEMON);

    get_op(argc, argv);

    if (server_conf.runmode == RUN_DAEMON) {
        if (daemonize() < 0) {
            syslog(LOG_ERR, "daemonize(): %s", strerror(errno));
            exit(1);
        }
    } else if (server_conf.runmode == RUN_FOREGROUND) {
    } else {
        syslog(LOG_ERR, "unknow runmode");
        exit(1);
    }

    // 套接字初始化
    socket_init();

    int list_size;
    int i;

    // 获取频道信息
    if (mlib_getchnlist(&list, &list_size) < 0) {
        syslog(LOG_ERR, "mlib_getchnlist()\n");
        exit(1);
    }

    // 节目单线程
    if (thr_list_create(list, list_size) < 0) {
        syslog(LOG_ERR, "thr_list_create()\n");
        exit(1);
    }

    // 频道线程
    for (i = 0; i < list_size; i++) {
        if (thr_channel_create(list + i) < 0) {
            syslog(LOG_ERR, "thr_channel_create()\n");
            exit(1);
        }
    }

    while (1)
        pause();

    exit(0);
}

static int daemonize()
{
    pid_t pid;
    int fd;

    pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        if ((fd = open("/dev/null", O_RDWR)) == -1) {
            return -1;
        }
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2)
            close(fd);
        setsid();
        // 保证存在路径
        chdir("/");
        return 0;
    } else {
        exit(0);
    }
    return 0;
}

static void get_op(int argc, char* argv[])
{
    int choice;
    while (1) {
        choice = getopt(argc, argv, "M:P:FD:I:H");

        if (choice == -1)
            break;

        switch (choice) {
        case 'M':
            server_conf.mgroup = optarg;
            break;
        case 'P':
            server_conf.rcvport = optarg;
            break;
        case 'F':
            server_conf.runmode = RUN_FOREGROUND;
            break;
        case 'D':
            server_conf.media_dir = optarg;
            break;
        case 'I':
            server_conf.ifname = optarg;
            break;
        case 'H':
            print_help();
            exit(0);
            break;
        default:
            abort();
            break;
        }
    }
}

static void print_help()
{
    printf("-M     指定多播组\n");
    printf("-P     接受端口\n");
    printf("-F     前台运行\n");
    printf("-D     媒体库\n");
    printf("-I     网络设备\n");
    printf("-H     帮助\n");
}

#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

struct client_conf_st client_conf = {
    .rcvport = DEFAULT_PORT,
    .mgroup = DEFAULT_IP,
    .player_cmd = PLAYER_CMD
};

static void get_cmd(int argc, char** argv);
static void printf_help();
static int get_ifname(char**);
static ssize_t writen(int fd, const void* buf, size_t n);
static void send_to_mpg123(int sd, int* pd);
static void revc_from_socket(int sd, int* pd);

// cmd > env > conf > default
int main(int argc, char** argv)
{
    int sd;
    int val;
    struct ip_mreqn mreqn;
    struct sockaddr_in laddr;
    int pd[2];
    int pid;
    int i = 0, j;

    // 获取命令行参数
    get_cmd(argc, argv);

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }

    // 多播组应和服务端多播组一致
    if (inet_pton(AF_INET, client_conf.mgroup, &mreqn.imr_multiaddr) <= 0) {
        perror("inet_pton()");
        exit(1);
    }
    // 本地地址
    if (inet_pton(AF_INET, "0.0.0.0", &mreqn.imr_address) <= 0) {
        perror("inet_pton()");
        exit(1);
    }
    char* ifname = NULL;
    if (get_ifname(&ifname) < 0) {
        perror("get_ifname");
        exit(1);
    }
    mreqn.imr_ifindex = if_nametoindex(ifname);

    // 加入多播组
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreqn, sizeof(mreqn)) < 0) {
        perror("setsockopt()");
        exit(1);
    }

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(client_conf.rcvport));
    if (inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr) <= 0) {
        perror("inet_pton()");
        exit(1);
    }
    // 绑定服务器设置的端口和本地地址
    if (bind(sd, (void*)&laddr, sizeof(laddr)) < 0) {
        perror("bind()");
        exit(1);
    }

    if (pipe(pd) < 0) {
        perror("pipe()");
        exit(1);
    }

    switch (fork()) {
    case -1:
        perror("fork()");
        exit(1);
        break;
    case 0:
        send_to_mpg123(sd, pd);
        break;
    default:
        revc_from_socket(sd, pd);
        break;
    }

    exit(0);
}

static void get_cmd(int argc, char** argv)
{
    int choice;
    while (1) {
        static struct option long_options[] = {
            { "port", required_argument, 0, 'P' },
            { "mgroup", required_argument, 0, 'M' },
            { "player", required_argument, 0, 'p' },
            { "help", no_argument, 0, 'H' },
            { 0, 0, 0, 0 }
        };

        int option_index = 0;

        choice = getopt_long(argc, argv, "P:M:p:H", long_options, &option_index);

        if (choice == -1)
            break;

        switch (choice) {
        case 'P':
            client_conf.rcvport = optarg;
            break;
        case 'M':
            client_conf.mgroup = optarg;
            break;
        case 'p':
            client_conf.player_cmd = optarg;
            break;
        case 'H':
            printf_help();
            exit(0);
        default:
            abort();
            break;
        }
    }
}

static void printf_help()
{
    printf("-P --port       接受端口\n");
    printf("-M --mgroup     指定多播组\n");
    printf("-p --player     播放器\n");
    printf("-H --help       帮助\n");
}

static int get_ifname(char** ifname)
{
    struct sockaddr_in* sin = NULL;
    struct ifaddrs *ifa = NULL, *ifList;

    if (getifaddrs(&ifList) < 0) {
        errno = ENXIO;
        return -1;
    }

    for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                ifname[0] = malloc(100);
                strcpy(ifname[0], ifa->ifa_name);
                break;
            }
            continue;
        }
    }
    freeifaddrs(ifList);

    return 0;
}

static ssize_t writen(int fd, const void* buf, size_t n)
{
    int len = 0;
    int pos = 0;
    while (n > 0) {
        len = write(fd, buf + pos, n);
        if (len < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        n -= len;
        pos += len;
    }
    return pos;
}

static void send_to_mpg123(int sd, int* pd)
{
    close(sd);
    close(pd[1]);
    // 重定向到标准输入
    dup2(pd[0], 0);
    if (pd[0] > 0)
        close(pd[0]);
    execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
    perror("execl()");
    exit(1);
}

static void revc_from_socket(int sd, int* pd)
{
    struct sockaddr_in serveraddr, raddr;
    socklen_t raddr_len, serveraddr_len;
    int chnids[CHNNR];
    int chosenid;
    int len;
    int i = 0, j = 0;

    close(pd[0]);
    struct msg_list_st* msg_list;
    msg_list = malloc(MSG_LIST_MAX);
    if (msg_list == NULL) {
        perror("malloc()");
        exit(1);
    }

    while (1) {
        len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0,
            (void*)&serveraddr, &serveraddr_len);
        if (len < sizeof(struct msg_list_st)) {
            /*fprintf(stderr, "client.c => recvfrom()[list]: too small, skip\n");*/
            continue;
        }
        if (msg_list->chnid != 0) {
            /*fprintf(stderr, "client.c => recvfrom()[list]: not list, skip\n");*/
            continue;
        } else {
            /*fprintf(stderr, "client.c => recvfrom()[list]: list, break\n");*/
            break;
        }
    }

    struct msg_listentry_st* entry_pos;
    struct msg_channel_st* msg_channel;
    chnid_t* id = mmap(0, sizeof(chnid_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (id == NULL) {
        perror("mmap()");
        exit(1);
    }
    *id = 1;

    switch (fork()) {
    case -1:
        perror("fork()");
        exit(1);
        break;
    case 0:
        // 获取用户输入进程
        while (1) {
            for (entry_pos = msg_list->entrys; (char*)entry_pos < ((char*)msg_list + len);
                 entry_pos = (struct msg_listentry_st*)((char*)entry_pos + entry_pos->len)) {
                printf("channel %d: %s", entry_pos->chnid, entry_pos->describe);
                chnids[i++] = entry_pos->chnid;
            }
            printf("\nInput Channel ID: ");
            scanf("%d", &chosenid);
            for (j = 0; j < i; j++) {
                if (chosenid == chnids[j]) {
                    *id = chosenid;
                    break;
                }
            }
            if (j == i)
                printf("Input error!\n");
        }
        break;
    default:
        msg_channel = malloc(MSG_CHANNEL_MAX);
        if (msg_channel == NULL) {
            perror("malloc()");
            exit(1);
        }
        while (1) {
            len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0,
                (void*)&raddr, &raddr_len);
            // 地址不同
            /*if (raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr*/
            /*|| raddr.sin_port != serveraddr.sin_port) {*/
            /*fprintf(stderr, "client.c => recvfrom()[channel]: raddr != serveraddr\n");*/
            /*continue;*/
            /*}*/
            if (len < sizeof(struct msg_channel_st)) {
                fprintf(stderr, "client.c => recvfrom()[channel]: too small, skip\n");
                continue;
            }
            if (msg_channel->chnid == *id) {
                if (writen(pd[1], msg_channel->data, len - sizeof(chnid_t)) < 0) {
                    perror("writen()");
                    exit(1);
                }
            }
        }
        break;
    }

    munmap(id, sizeof(chnid_t));
    free(msg_list);
    free(msg_channel);
    close(sd);
}

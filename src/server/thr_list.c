#include "thr_list.h"
#include "medialib.h"
#include "server_conf.h"
#include <bits/stdint-uintn.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static pthread_t tid_list;
// 频道总数
static int nr_list_ent;
static struct mlib_listentry_st* list_ent;

static void* thr_list(void* ptr)
{
    int totalsize;
    uint8_t size;
    struct msg_listentry_st* msg_listentry;
    struct msg_list_st* msg_list;
    int i;
    ssize_t ret;

    totalsize = sizeof(chnid_t);
    for (i = 0; i < nr_list_ent; i++)
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent->desc);

    msg_list = malloc(totalsize);
    if (msg_list == NULL) {
        syslog(LOG_ERR, "thr_list.c => malloc(): %s", strerror(errno));
        exit(1);
    }

    // 发送频道信息的频道号为0
    msg_list->chnid = LISTCHNID;
    msg_listentry = msg_list->entrys;

    for (i = 0; i < nr_list_ent; i++) {
        size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
        msg_listentry->chnid = list_ent[i].chnid;
        msg_listentry->len = size;
        strcpy((char*)msg_listentry->describe, list_ent[i].desc);
        // 转到下一条记录地址
        msg_listentry = (void*)((char*)msg_listentry + size);
    }

    // 发送节目单
    while (1) {
        ret = sendto(sd, msg_list, totalsize, 0, (void*)&snaddr, sizeof(snaddr));
        if (ret < 0) {
            syslog(LOG_ERR, "thr_list.c => sendto(): %s", strerror(errno));
        } else {
            syslog(LOG_DEBUG, "thr_list.c => sendto(): list successed");
        }
        sleep(1);
    }

    pthread_exit(NULL);
}

// 创建线程发送频道信息
int thr_list_create(struct mlib_listentry_st* list, int list_size)
{
    list_ent = list;
    nr_list_ent = list_size;

    if (pthread_create(&tid_list, NULL, thr_list, NULL)) {
        syslog(LOG_ERR, "thr_list.c => pthread_create(): %s", strerror(errno));
        return -1;
    }

    return 0;
}

int thr_list_destroy()
{
    if(pthread_cancel(tid_list))
        syslog(LOG_ERR, "thr_list.c => pthread_cancel(): failed");
    pthread_join(tid_list, NULL);
    return 0;
}

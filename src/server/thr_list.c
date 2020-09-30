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

static pthread_t tid_list;
static int nr_list_ent;
static struct mlib_listentry_st* list_ent;

static void* thr_list(void* p)
{
    int totalsize, size;
    struct msg_listentry_st* msg_listentry;
    struct msg_list_st* msg_list;
    int i;
    ssize_t ret;

    totalsize = sizeof(chnid_t);
    for (i = 0; i < nr_list_ent; i++) {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent->desc);
    }

    msg_list = malloc(totalsize);
    if (msg_list == NULL) {
        syslog(LOG_ERR, "malloc(): %s", strerror(errno));
        exit(1);
    }

    msg_list->chnid = LISTCHNID;
    msg_listentry = msg_list->entrys;

    for (i = 0; i < nr_list_ent; i++) {
        size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);

        msg_listentry->chnid = list_ent[i].chnid;
        msg_listentry->len = htons(size);
        strcpy((char*)msg_listentry->describe, list_ent[i].desc);
        // 转到下一条记录地址
        msg_listentry = (void*)((char*)msg_listentry + size);
    }

    struct timespec rq;
    rq.tv_sec = 0;
    rq.tv_nsec = 10000000;
    // 发送节目单
    while (1) {
        ret = sendto(sd, msg_list, totalsize, 0, (void*)&snaddr, sizeof(snaddr));
        if (ret < 0) {
            syslog(LOG_WARNING, "sendto(): %s", strerror(errno));
        } else {
            syslog(LOG_DEBUG, "sendto() successed");
        }

        nanosleep(&rq, NULL);
    }

    pthread_exit(NULL);
}

int thr_list_create(struct mlib_listentry_st* list, int list_size)
{
    list_ent = list;
    nr_list_ent = list_size;
    if (pthread_create(&tid_list, NULL, thr_list, NULL)) {
        syslog(LOG_ERR, "pthread_create(): %s", strerror(errno));
        return -1;
    }

    return 0;
}

int thr_list_destroy()
{
    pthread_cancel(tid_list);
    pthread_join(tid_list, NULL);
    return 0;
}

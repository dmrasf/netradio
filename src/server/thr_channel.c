#include "thr_channel.h"
#include "medialib.h"
#include "server_conf.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>

struct thr_channel_ent_st {
    chnid_t chnid;
    pthread_t tid;
};

static struct thr_channel_ent_st thr_channels[CHNNR];
// 创建成功的线程的下标
static int pos;

static void* thr_channel_snder(void* ptr)
{
    struct msg_channel_st* msg_channel;
    struct mlib_listentry_st* mlib_listent = ptr;
    int len;
    int datasize;

    msg_channel = malloc(MSG_CHANNEL_MAX);
    if (msg_channel == NULL) {
        syslog(LOG_ERR, "thr_channel.c => malloc(): %s", strerror(errno));
        exit(1);
    }
    msg_channel->chnid = mlib_listent->chnid;

    while (1) {
        len = mild_readchn(mlib_listent->chnid, msg_channel->data, MAX_DATA);
        if (sendto(sd, msg_channel, len + sizeof(chnid_t), 0, (void*)&snaddr, sizeof(snaddr)) < 0) {
            syslog(LOG_ERR, "thr_channel.c => sendto(): channel %d failed %s(len = %d)", mlib_listent->chnid, strerror(errno), len);
        }
        syslog(LOG_DEBUG, "thr_channel.c => sendto(): channel %d successed(len = %d)", mlib_listent->chnid, len);
        sched_yield();
    }

    pthread_exit(NULL);
}

int thr_channel_create(struct mlib_listentry_st* mlib_listentry)
{
    int err;

    err = pthread_create(&thr_channels[pos].tid, NULL, thr_channel_snder, mlib_listentry);
    if (err) {
        syslog(LOG_ERR, "thr_channel.c => pthread_create(): %d failed", mlib_listentry->chnid);
        return -err;
    }
    thr_channels[pos].chnid = mlib_listentry->chnid;
    syslog(LOG_DEBUG, "thr_channel.c => pthread_create(): %d created", mlib_listentry->chnid);
    // 位置加一  为下一个频道准备
    pos++;

    return 0;
}

int thr_channel_destroy(struct mlib_listentry_st* ptr)
{
    int i;
    for (i = 0; i < CHNNR; i++) {
        if (thr_channels[i].chnid == ptr->chnid) {
            if (pthread_cancel(thr_channels[i].tid) < 0) {
                syslog(LOG_ERR, "thr_channel.c => pthread_cancel(): tid = %d", i);
                return ESRCH;
            }
            pthread_join(thr_channels[i].tid, NULL);
            thr_channels[i].chnid = -1;
            break;
        }
    }
    return 0;
}

int thr_channel_destroyall()
{
    int i;
    for (i = 0; i < CHNNR; i++) {
        if (thr_channels[i].chnid != 0) {
            if (pthread_cancel(thr_channels[i].tid) < 0) {
                syslog(LOG_ERR, "thr_channel.c => pthread_cancel(): tid = %d", i);
                return ESRCH;
            }
            pthread_join(thr_channels[i].tid, NULL);
            thr_channels[i].chnid = -1;
            break;
        }
    }
    return 0;
}

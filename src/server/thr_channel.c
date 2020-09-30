#include "thr_channel.h"
#include "medialib.h"
#include "server_conf.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>

struct thr_channel_ent_st {
    chnid_t chnid;
    pthread_t tid;
};

static struct thr_channel_ent_st thr_channels[CHNNR];
static int pos;

static void* thr_channel_snder(void* ptr)
{
    struct msg_channel_st* msg_channel;
    struct mlib_listentry_st* mlib_listent = ptr;
    int len;
    int datasize;

    msg_channel = malloc(MSG_CHANNEL_MAX);
    if (msg_channel == NULL) {
        syslog(LOG_ERR, "malloc(): %s", strerror(errno));
        exit(1);
    }
    msg_channel->chnid = mlib_listent->chnid;

    while (1) {
        len = mild_readchn(mlib_listent->chnid, msg_channel->data, BITRATE);
        sendto(sd, msg_channel->data, len, 0, (void*)&snaddr, sizeof(snaddr));
        sched_yield();
    }

    pthread_exit(NULL);
}

int thr_channel_create(struct mlib_listentry_st* mlib_listentry)
{
    int err;

    err = pthread_create(&thr_channels[pos].tid, NULL, thr_channel_snder, mlib_listentry);
    if (err) {
        syslog(LOG_ERR, "pthread_create()");
        return -err;
    }
    thr_channels[pos].chnid = mlib_listentry->chnid;
    pos++;

    return 0;
}

int thr_channel_destroy(struct mlib_listentry_st* ptr)
{
    int i;
    for (i = 0; i < CHNNR; i++) {
        if (thr_channels[i].chnid == ptr->chnid) {
            if (pthread_cancel(thr_channels[i].tid) < 0) {
                syslog(LOG_ERR, "pthread_cancel(): %d thr", i);
                return -ESRCH;
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
                syslog(LOG_ERR, "pthread_cancel(): %d thr", i);
                return -ESRCH;
            }
            pthread_join(thr_channels[i].tid, NULL);
            thr_channels[i].chnid = -1;
            break;
        }
    }
    return 0;
}

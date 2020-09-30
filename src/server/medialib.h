#ifndef MEDIA_H__
#define MEDIA_H__

#include "../include/proto.h"
#include <sys/types.h>

#define PATHSIZE 1024
#define BITRATE (16 * 1024)

struct mlib_listentry_st {
    chnid_t chnid;
    char* desc;
};

int mlib_getchnlist(struct mlib_listentry_st**, int*);

int mlib_freechnlist(struct mlib_listentry_st*);

ssize_t mild_readchn(chnid_t, void*, size_t);

#endif

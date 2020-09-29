#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"
#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

#define PATHSIZE 1024

struct channel_context_st {
    chnid_t chnid;
    char* desc;
    glob_t mp3glob;
    int fd;
    off_t offset;
    mytbf_t* mytbf;
};

static struct channel_context_st channel[MAXCHNID + 1];
static int path2entry(char* path, int* num, struct mlib_listentry_st* ptr)
{
    glob_t globres;

    // 对每个频道目录解析 掠过无效 保存到channel中
    if (glob(path, 0, NULL, &globres)) {
        return -1;
    }


    ptr[*num].chnid = 1;
    ptr[*num].desc = "edw";
    (*num)++;

    return 0;
}

// 获取频道
int mlib_getchnlist(struct mlib_listentry_st** list, int* list_size)
{
    int i;
    char path[PATHSIZE];
    glob_t globres;
    int num = 0;
    struct mlib_listentry_st* ptr;

    for (i = 0; i < MAXCHNID + 1; i++) {
        channel[i].chnid = -1;
    }

    snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);

    if (glob(path, 0, NULL, &globres)) {
        return -1;
    }

    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if (ptr == NULL) {
        syslog(LOG_ERR, "malloc()");
        exit(1);
    }

    for (i = 0; i < globres.gl_pathc; i++) {
        if (path2entry(globres.gl_pathv[i], &num, ptr) < 0) {
            syslog(LOG_ERR, "path2entry()");
            exit(1);
        }
    }

    *list = ptr;
    *list_size = num;

    return 0;
}

// 从制定频道读取数据
ssize_t mild_readchn(chnid_t chnid, void* ptr, size_t n)
{
    int fd = channel[chnid].fd;

    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n)
                return (-1); /* error, return -1 */
            else
                break; /* error, return amount read so far */
        } else if (nread == 0) {
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

int mlib_freechnlist(struct mlib_listentry_st* list)
{
    free(list);
    return 0;
}

#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

struct channel_context_st {
    chnid_t chnid;
    char* desc;
    glob_t mp3glob;
    int fd;
    int pos;
    mytbf_t* mytbf;
};

static struct channel_context_st channel[MAXCHNID + 1];
static int path2entry(char* path, int* num, struct mlib_listentry_st* ptr)
{
    glob_t descglob, mp3glob;
    char descpath[PATHSIZE];
    char mp3path[PATHSIZE];
    int i;
    FILE* fp;
    int chnid = -1;
    char* line = NULL;
    size_t linesize = 0;

    snprintf(descpath, PATHSIZE, "%s/*.txt", path);
    // 对每个频道目录解析 掠过无效 保存到channel中
    if (glob(descpath, 0, NULL, &descglob)) {
        syslog(LOG_WARNING, "desc no match");
        return 0;
    }
    snprintf(mp3path, PATHSIZE, "%s/*.mp3", path);
    if (glob(mp3path, 0, NULL, &mp3glob)) {
        syslog(LOG_WARNING, "mp3 no match");
        return 0;
    }

    syslog(LOG_DEBUG, "%s", descglob.gl_pathv[0]);
    syslog(LOG_DEBUG, "%s", mp3glob.gl_pathv[0]);

    // 解析描述文件
    fp = fopen(descglob.gl_pathv[0], "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "fopen");
        return -1;
    }

    if (getline(&line, &linesize, fp) < 0) {
        syslog(LOG_ERR, "getline ch num");
        return -1;
    }
    chnid = atoi(line);

    syslog(LOG_DEBUG, "chnid = %d, desc = %s", chnid, line);
    
    if (chnid < 1 || chnid > MAXCHNID) {
        syslog(LOG_ERR, "get chnid");
        return -1;
    }

    syslog(LOG_DEBUG, "chnid = %d, desc = %s", chnid, line);

    channel[chnid].chnid = chnid;
    if (getline(&line, &linesize, fp) < 0) {
        syslog(LOG_ERR, "get desc");
        return -1;
    }

    syslog(LOG_DEBUG, "chnid = %d, desc = %s", chnid, line);

    channel[chnid].desc = line;
    // 16Kb = 16*8Kbit = 128Kbit
    channel[chnid].mytbf = mytbf_init(BITRATE, BITRATE * 5);
    channel[chnid].mp3glob = mp3glob;
    channel[chnid].pos = 0;
    channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], O_RDONLY);
    ptr[*num].chnid = chnid;
    ptr[*num].desc = line;
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
        syslog(LOG_DEBUG, "%s", path);
        return -1;
    }

    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if (ptr == NULL) {
        syslog(LOG_ERR, "malloc()");
        exit(1);
    }

    for (i = 0; i < globres.gl_pathc; i++) {
        syslog(LOG_DEBUG, "%s", globres.gl_pathv[i]);
        if (path2entry(globres.gl_pathv[i], &num, ptr) < 0) {
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
    size_t nleft;
    ssize_t nread;
    int ret;
    // n bitrate
    ret = mytbf_fetchtoken(channel[chnid].mytbf, n);

    nleft = ret;
    while (nleft > 0) {
        if ((nread = read(channel[chnid].fd, ptr, nleft)) < 0) {
            if (nleft == n)
                return (-1); /* error, return -1 */
            else
                break; /* error, return amount read so far */
        } else if (nread == 0) {
            // 若读完一个文件，则打开下一个文件继续
            close(channel[chnid].fd);
            syslog(LOG_DEBUG, "finish");
            channel[chnid].pos = (channel[chnid].pos + 1) % channel[chnid].mp3glob.gl_pathc;
            channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], O_RDONLY);
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (ret - nleft); /* return >= 0 */
}

int mlib_freechnlist(struct mlib_listentry_st* list)
{
    channel[list->chnid].chnid = -1;
    close(channel[list->chnid].fd);
    mytbf_destroy(channel[list->chnid].mytbf);
    free(list->desc);
    free(list);
    return 0;
}

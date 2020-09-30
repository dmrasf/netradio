#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static struct channel_context_st channels[MAXCHNID + 1];

// 根据每一条路径解析频道数据
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
    // 对每个频道目录解析 掠过无效 保存到channels中
    if (glob(descpath, 0, NULL, &descglob)) {
        syslog(LOG_WARNING, "medialib.c => glob(): desc not exit!");
        return 0;
    }
    snprintf(mp3path, PATHSIZE, "%s/*.mp3", path);
    if (glob(mp3path, 0, NULL, &mp3glob)) {
        syslog(LOG_WARNING, "medialib.c => glob(): mp3 not exit!");
        return 0;
    }

    /*syslog(LOG_DEBUG, "%s", descglob.gl_pathv[0]);*/
    /*syslog(LOG_DEBUG, "%s", mp3glob.gl_pathv[0]);*/

    // 解析描述文件
    fp = fopen(descglob.gl_pathv[0], "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "medialib.c => fopen(): %s", strerror(errno));
        return -1;
    }

    if (getline(&line, &linesize, fp) < 0) {
        syslog(LOG_ERR, "medialib.c => getline(): get channel id failed");
        return -1;
    }

    chnid = atoi(line);
    if (chnid < 1 || chnid > MAXCHNID) {
        syslog(LOG_ERR, "medialib.c => atoi(): get error channel id(%d)", chnid);
        return -1;
    }
    channels[chnid].chnid = chnid;

    if (getline(&line, &linesize, fp) < 0) {
        syslog(LOG_ERR, "medialib.c => getline(): get desc failed");
        return -1;
    }

    /*syslog(LOG_DEBUG, "chnid = %d, desc = %s", chnid, line);*/

    // 16Kb = 16*8Kbit = 128Kbit
    channels[chnid].mytbf = mytbf_init(BITRATE, BITRATE * 5);
    channels[chnid].desc = line;
    channels[chnid].mp3glob = mp3glob;
    // 表示频道正在播放的歌曲位置
    channels[chnid].pos = 0;
    channels[chnid].fd = open(channels[chnid].mp3glob.gl_pathv[channels[chnid].pos], O_RDONLY);
    ptr[*num].chnid = chnid;
    ptr[*num].desc = line;
    // 只有解析正确时跳到下一个位置
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
    int err;
    char* globmsg;

    // 初始化频道
    for (i = 0; i < MAXCHNID + 1; i++)
        channels[i].chnid = -1;

    snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);

    err = glob(path, 0, NULL, &globres);
    if (err) {
        switch (err) {
        case GLOB_NOSPACE:
            globmsg = "running out of memory";
            break;
        case GLOB_ABORTED:
            globmsg = "a read error";
            break;
        case GLOB_NOMATCH:
            globmsg = "no found matches";
            break;
        default:
            break;
        }
        syslog(LOG_ERR, "medialib.c => glob(): %s failed %s", path, globmsg);
        return -1;
    }

    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if (ptr == NULL) {
        syslog(LOG_ERR, "medialib.c => malloc(): %s", strerror(errno));
        return -1;
    }

    for (i = 0; i < globres.gl_pathc; i++) {
        syslog(LOG_DEBUG, "%s", globres.gl_pathv[i]);
        // 除非出现特别大的错误，否则会跳过出错的频道
        if (path2entry(globres.gl_pathv[i], &num, ptr) < 0) {
            syslog(LOG_ERR, "medialib.c => path2entry(): failed");
            exit(1);
        }
    }

    *list = ptr;
    *list_size = num;

    return 0;
}

// 当读完或读出错时，打开下一首歌
static void open_next_song(chnid_t chnid)
{
    close(channels[chnid].fd);
    channels[chnid].pos = (channels[chnid].pos + 1) % channels[chnid].mp3glob.gl_pathc;
    channels[chnid].fd = open(channels[chnid].mp3glob.gl_pathv[channels[chnid].pos], O_RDONLY);
    if (channels[chnid].fd < 0) {
        syslog(LOG_ERR, "medialib.c => open_next_song(): %s", strerror(errno));
        exit(1);
    }
}

// 从制定频道读取数据
ssize_t mild_readchn(chnid_t chnid, void* ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    int tokenret;
    int readret;

    // mytbf_fetchtoken 每次获取128Kbit
    tokenret = mytbf_fetchtoken(channels[chnid].mytbf, n);

    while (1) {
        readret = read(channels[chnid].fd, ptr, tokenret);
        // 读取失败
        if (readret < 0) {
            syslog(LOG_ERR, "medialib.c => read(): channel %d %s", chnid, strerror(errno));
            open_next_song(chnid);
        } else if (readret == 0) {
            syslog(LOG_DEBUG, "medialib.c => read(): channel %d, play next!", chnid);
            open_next_song(chnid);
        } else {
            mytbf_returntoken(channels[chnid].mytbf, tokenret - readret);
            break;
        }
    }
    syslog(LOG_DEBUG, "channel %d: want %ld, actully %d", chnid, n, readret);
    return readret;
}

int mlib_freechnlist(struct mlib_listentry_st* list)
{
    // 清空频道号数组
    channels[list->chnid].chnid = -1;
    // 关闭频道下的文件描述符
    close(channels[list->chnid].fd);
    mytbf_destroy(channels[list->chnid].mytbf);
    free(list);
    return 0;
}

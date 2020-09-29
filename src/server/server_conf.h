#ifndef SERVER_CONF_H__
#define SERVER_CONF_H__

#include <netinet/in.h>

#define DEFAULT_MEDIADIR "/var/medir"
#define DEFAULT_IF "wlp3s0"

enum {
    RUN_DAEMON = 1,
    RUN_FOREGROUND
};

struct server_conf_st {
    char* rcvport;
    char* mgroup;
    char* media_dir;
    char runmode;
    char* ifname;
};

extern struct server_conf_st server_conf;
extern int sd;
extern struct sockaddr_in snaddr;

#endif

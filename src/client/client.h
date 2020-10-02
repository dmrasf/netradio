#ifndef CLIENT_H__
#define CLIENT_H__

#include <proto.h>

#define PLAYER_CMD "(/usr/bin/mpg123 - > /dev/null 2>&1)"

struct client_conf_st {
    char* rcvport;
    char* mgroup;
    char* player_cmd;
};

extern struct client_conf_st client_conf;

#endif

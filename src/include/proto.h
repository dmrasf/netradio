#ifndef PROTO_H__macro
#define PROTO_H__

#include "site_type.h"

#define DEFAULT_IP "224.1.1.2"
#define DEFAULT_PORT "3333"

// channel num
#define CHNNR 100

// for channel list
#define LISTCHNID 0
#define MINCHNID 1
#define MAXCHNID (MINCHNID + CHNNR - 1)

#define MSG_CHANNEL_MAX (65536 - 20 - 8)
#define MAX_DATA (MSG_CHANNEL_MAX - sizeof(chnid_t))

#define MSG_LIST_MAX (65536 - 20 - 8)
#define MAX_ENTRY (MSG_LIST_MAX - sizeof(chnid_t))

struct msg_channel_st {
    chnid_t chnid;
    uint8_t data[1];
} __attribute__((packed));

struct msg_listentry_st {
    chnid_t chnid;
    uint8_t len;
    uint8_t describe[1];
} __attribute__((packed));

struct msg_list_st {
    chnid_t chnid;
    struct msg_listentry_st entrys[1];
} __attribute__((packed));

#endif

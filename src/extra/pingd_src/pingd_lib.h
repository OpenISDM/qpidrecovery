/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

int pingd_socket(int isclient);
int pingd_receivefrom(int sfd, char* from, void* buf, int bufsize);
int pingd_sendto(int sfd, const char* ipaddress, void* buf, int bufsize, int isclient);

#define MAX_ADDRESS_LENGTH (40)
#include <time.h>

struct PingdHops{
    int hops;
    char from[MAX_ADDRESS_LENGTH];
    time_t timestamp; // time stamp before sending
};
#define HOPS_ERROR (-1)

struct PingdPing{
    char to[MAX_ADDRESS_LENGTH];
};


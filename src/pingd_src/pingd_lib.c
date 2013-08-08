#include"pingd_lib.h"
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

static const uint16_t pingdport = 5600;
static const uint16_t clientport = 5601;

static int fillsockaddr_in(struct sockaddr_in* addr, const char* ipaddr, const uint16_t port){
    addr->sin_family = AF_INET;
    if(ipaddr != NULL){
        if(inet_pton(AF_INET, ipaddr, &(addr->sin_addr)) <= 0)
            return -1;
    }
    else addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_port = htons(port);
    return 0;
}

int pingd_socket(int isclient){
    int sfd;
    struct sockaddr_in addr;
    if(fillsockaddr_in(&addr, NULL, isclient? clientport: pingdport) == -1)
        return -1;

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd == -1)
        return -1;
    if(bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        close(sfd);
        return -1;
    }
    return sfd;
}

int pingd_receivefrom(int sfd, char* from, void* buf, int bufsize){
    struct sockaddr_in addr;
    socklen_t addrsize = sizeof(addr);

    if(recvfrom(sfd, buf, bufsize, 0, (struct sockaddr*)&addr, &addrsize) == -1)
        return -1;
    if(inet_ntop(AF_INET, &(addr.sin_addr), from, MAX_ADDRESS_LENGTH) == NULL)
        return -1;
    return 0;
}

int pingd_sendto(int sfd, const char* ipaddress, void* buf, int bufsize, int isclient){
    struct sockaddr_in addr;
    if(fillsockaddr_in(&addr, ipaddress, isclient? pingdport: clientport) == -1)
        return -1;
    if(sendto(sfd, buf, bufsize, 0, (struct sockaddr*)&addr, sizeof(addr)) != bufsize)
        return -1;
    return 0;
}


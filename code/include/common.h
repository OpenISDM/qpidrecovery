#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

/*
ping daemon=5600
ping client=5601
*/
// proposer.cpp
#define REQUEST_PROPOSER_PORT (5610)
#define QUERY_BACKUP_PORT     (5611)

// acceptor.cpp
#define REQUEST_ACCEPTOR_PORT (5620)
#define PAXOS_PORT            (5621)

// requestor.cpp
#define QUERY_PROPOSER_PORT   (5630)

// heartbeat.cpp
#define HEARTBEAT_PORT        (5640)

// bgtraffic.cpp
#define BGTRAFFIC_PORT        (5650)

//messagecopy.cpp
#define MESSAGECOPY_PORT      (5660)

#define SERVICE_NAME_LENGTH (40)
#define IP_LENGTH (20)

struct Heartbeat{
	char name[SERVICE_NAME_LENGTH];
};

struct EmptyMessage{
	int value;
};

#define MAX_PROPOSERS (9)
#define MAX_ACCEPTORS (MAX_PROPOSERS * 2 - 1)
struct ParticipateRequest{
	char name[SERVICE_NAME_LENGTH]; // name of the requestor
	unsigned brokerport;
	unsigned votingversion;
	unsigned committedversion; // of AcceptorChangePropsal
	unsigned length;
	char acceptor[MAX_ACCEPTORS][IP_LENGTH];
};

struct ReplyProposer{
	char name[SERVICE_NAME_LENGTH];
	unsigned length;
	char ip[MAX_PROPOSERS][IP_LENGTH];
};

struct ReplyAddress{
	char name[SERVICE_NAME_LENGTH];
	char ip[IP_LENGTH];
	unsigned port;
};

struct CopyMessageRequest{
	char sourceip[IP_LENGTH];
	unsigned sourceport;
	char targetip[IP_LENGTH];
	unsigned targetport;
	char sourcename[64];
	char targetname[64];
};

/*
proposer.cpp, acceptor.cpp messagecopy.cpp
accept connection, read 1 message, and return new socket
*/
#include"socketlib.h"
#include<unistd.h>
#include<iostream>
template<typename T>
int acceptRead(int sfd, char *ip, T &msg){
	int newsfd;
	newsfd = tcpaccept(sfd, ip);
	if(newsfd < 0){
		std::cerr << "tcpaccept error\n";
		return -1;
	}
	if(read(newsfd, &msg, sizeof(T)) != sizeof(T)){
		std::cerr << "read error\n";
		close(newsfd);
		return -1;
	}
	return newsfd;
}

#ifndef STDCOUT
#define STDCOUT(T) (std::cout << T)
#define STDCOUTFLUSH() (std::cout.flush())
#endif

//#define DELAY()
#define DELAY() usleep(100)

#endif


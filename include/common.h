/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice at the end of this file
*/

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

#define MAX_PROPOSERS (3)
#define MAX_ACCEPTORS (MAX_PROPOSERS * 2 - 1)
struct ParticipateRequest{
	char name[SERVICE_NAME_LENGTH]; // name of the requestor
	char brokerip[IP_LENGTH];
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

/******************************************************************************
* Copyright (c) 2013 newslab.csie.ntu.edu.tw, Taiwan
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/



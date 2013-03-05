#include"common.h"
#include"socketlib.h"
#include"fileselector.h"
#include<iostream>
#include<vector>

using namespace std;

//typedef vector<AcceptorStateMachine*> ASMVector;
typedef vector<int> ASMVector;

void createAcceptor(struct ParticipateRequest &req, char *from, ASMVector &asms){
	/*
	AcceptorStateMachine *asmp;
	asmp = new AcceptorStateMachine(req);
	asms.push_back(asmp);
	*/
}

int main(int argc,char *argv[]){
	int requestsfd, paxossfd;
	FileSelector fs(0,0);
	ASMVector asms;

	requestsfd = tcpserversocket(REQUEST_ACCEPTOR_PORT);
	paxossfd = tcpserversocket(PAXOS_PORT);
	if(requestsfd == -1 || paxossfd == -1){
		cerr << "error: request acceptor port\n";
		return -1;
	}

	fs.registerFD(requestsfd);
	fs.registerFD(paxossfd);
	while(1){
		int ready = fs.getReadReadyFD();

		if(ready == requestsfd){
			char from[IP_LENGTH];
			struct ParticipateRequest r;
			int newsfd;
			newsfd = acceptRead<struct ParticipateRequest>(requestsfd, from, r);
			if(newsfd >= 0){
				close(newsfd);
				createAcceptor(r, from, asms);
			}
			continue;
		}
		if(ready == paxossfd){
			// TODO: paxos
			continue;
		}
		if(ready == GET_READY_FD_TIMEOUT){
			fs.resetTimeout(1000,0);
			continue;
		}
		cerr << "getReadyFD error";
	}
}

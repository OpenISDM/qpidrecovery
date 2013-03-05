#include<string>
#include<iostream>
//#include"topology.h"
//#include"listener.h"
//#include"brokeraddress.h"
//#include"brokerobject.h"
#include<sys/time.h>
#include"common.h"
#include"fileselector.h"
#include<vector>
#include<cstring>

#include"memoryusage.h"
#include"socketlib.h"
#include"pingd_src/pingd_lib.h"
using namespace std;

//typedef vector<ProposerStateMachine*> PSMVector;
typedef vector<int> PSMVector;
typedef vector<struct QueryAddress> QAVector;

void createProposer(struct ParticipateRequest &req, char *from, PSMVector &psms){
	/*
	Qpid
	ProposerStateMachine *psmp;
	psmp = new ProposerStateMachine(req);
	psms.push_back(psmp);
	*/
}

int searchServiceByName(QAVector &namelist, struct QueryAddress &ret){
	for(QAVector::iterator i = namelist.begin(); i != namelist.end(); i++){
		if(strcmp((*i).name, ret.name) != 0)
			continue;
		ret = (*i);
		return 0;
	}
	return -1;
}

int main(int argc, char *argv[]){
	int requestsfd, querysfd;
	FileSelector fs(0,0);
	PSMVector psms;
	QAVector namelist;

	requestsfd = tcpserversocket(REQUEST_PROPOSER_PORT);
	querysfd = tcpserversocket(QUERY_BACKUP_PORT);

	fs.registerFD(requestsfd);
	fs.registerFD(querysfd);
	while(1){
		int ready = fs.getReadReadyFD();
		
		if(ready == requestsfd){
			char from[IP_LENGTH];
			struct ParticipateRequest r;
			int newsfd;
			newsfd = acceptRead<struct ParticipateRequest>(requestsfd, from, r);
			if(newsfd >= 0){
				close(newsfd);
				createProposer(r, from, psms);
			}
			continue;
		}
		if(ready == querysfd){
			char from[IP_LENGTH];
			struct QueryAddress r;
			int newsfd;
			newsfd = acceptRead<struct QueryAddress>(querysfd, from, r);
			if(newsfd >= 0){
				if(searchServiceByName(namelist, r) != -1)
					write(newsfd, &r, sizeof(struct QueryAddress));
				close(newsfd);
			}
			continue;
		}
		if(ready >= 0){
			//TODO: paxos
		}
		if(ready == GET_READY_FD_TIMEOUT){
			fs.resetTimeout(1000,0);
			continue;
		}
	}
}

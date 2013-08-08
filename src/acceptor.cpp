#include"common.h"
#include"socketlib.h"
#include"fileselector.h"
#include"paxos.h"
#include"timestamp.h"
#include<iostream>
#include<vector>
#include<cstring>

using namespace std;

class AcceptorName{
	public:
	AcceptorStateMachine *acc;
	char name[SERVICE_NAME_LENGTH];

	AcceptorName(AcceptorStateMachine *acceptor, const char* servicename){
		this->acc = acceptor;
		strcpy(this->name, servicename);
	}
};

typedef vector<AcceptorName> NVector;

static void createAcceptor(const char *name, unsigned votingver,
unsigned committedver, unsigned nacc, const char (*acc)[IP_LENGTH],
NVector &accname){
	for(NVector::iterator i = accname.begin(); i != accname.end(); i++){
		if(strcmp(name, (*i).name) == 0){
			delete (*i).acc;
			accname.erase(i);
			break;
		}
	}
	AcceptorName a(new AcceptorStateMachine(name, votingver,
	new AcceptorChangeProposal(committedver, nacc, acc)),
	name);
	accname.push_back(a);
}

static int handlePaxosMessage(int ready, NVector &accname){
	PaxosMessage m;
	if(m.receive(ready) < 0)
		return -1;
STDCOUT("acceptor: paxos msg\n");
	for(NVector::iterator i = accname.begin(); i != accname.end(); i++){
		const PaxosResult r = (*i).acc->handleMessage(ready, m);
		if(r == IGNORED)
			continue;
		if(r == HANDLED)
			return 0;
		cerr << "error: unknown paxos result\n";
	}
	return -1;
}

int main(int argc,char *argv[]){
	replaceSIGPIPE();
	if(argc >= 2)
		setLogName(argv[1]);

	int requestsfd, paxossfd;
	FileSelector fs(0, 1000);
	NVector accname;

	requestsfd = tcpserversocket(REQUEST_ACCEPTOR_PORT);
	paxossfd = tcpserversocket(PAXOS_PORT);
	if(requestsfd == -1 || paxossfd == -1){
		cerr << "error: port\n";
		return -1;
	}

	fs.registerFD(requestsfd);
	fs.registerFD(paxossfd);
	while(1){
printDot();
		int ready = fs.getReadReadyFD();

		if(ready == requestsfd){
STDCOUT("acceptor: requestfd " << ready << "\n");
			char from[IP_LENGTH];
			struct ParticipateRequest r;
			int newsfd;
			newsfd = acceptRead<struct ParticipateRequest>(requestsfd, from, r);
			if(newsfd >= 0){
				close(newsfd);
				createAcceptor(r.name, r.votingversion,
				r.committedversion, r.length, r.acceptor, accname);
			}
			continue;
		}
		if(ready == paxossfd){
STDCOUT("acceptor: new paxosfd\n");
			int newpaxossfd = tcpaccept(paxossfd, NULL);
			fs.registerFD(newpaxossfd);
			continue;
		}
		if(ready >= 0){
			if(handlePaxosMessage(ready, accname) == -1){
				fs.unregisterFD(ready);
				close(ready);
			}
			continue;
		}

		if(ready == GET_READY_FD_TIMEOUT){
STDCOUT("timeout\n");
			fs.resetTimeout(4, 0);

			for(NVector::iterator i = accname.begin(); i != accname.end(); i++){
				if((*i).acc->checkTimeout(4) == HANDLED){
STDCOUT("acceptor: state machine timeout\n");
				}
			}
			continue;
		}
		cerr << "getReadyFD error";
	}
}


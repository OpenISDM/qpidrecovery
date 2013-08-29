#include<vector>
#include<iostream>
#include<cstring>
#include<cstdio>

#include"socketlib.h"
#include"heartbeat_lib.h"
#include"namelist.h"
#include"fileselector.h"
#include"paxos.h"
#include"timestamp.h"

typedef vector<HeartbeatClient*> HBCVector;

using namespace std;

static unsigned copyIPArray(char (*ip)[IP_LENGTH], HBCVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		v[i]->getIP(ip[i]);
	}
	return v.size();
}

static int replyProposerList(int replysfd, const char *servicename, HBCVector &proposers){
	struct ReplyProposer r;
	memset(&r, 0, sizeof(r));
	strcpy(r.name, servicename);
	r.length = copyIPArray(r.ip, proposers);

	if(write(replysfd, &r, sizeof(r)) != sizeof(r)){
		cerr << "replyProposerList: write error\n";
		return -1;
	}
	return 0;
}

// -1 if error, 0 if ok
int sendRequest(const char *servicename, const char *ip, unsigned port, unsigned brokerport,
unsigned currentversion, HBCVector &acceptors){
	struct ParticipateRequest r;
	memset(&r, 0, sizeof(r));
	strcpy(r.name, servicename);
	r.votingversion = currentversion + 1;
	r.committedversion = currentversion;
	r.length = copyIPArray(r.acceptor, acceptors);
		r.brokerip[0] = '\0'; // how to get local ip?
	r.brokerport = brokerport;

	int sfd = tcpconnect(ip, port);
	if(sfd < 0)
		return -1;
	if(write(sfd, &r, sizeof(r)) != sizeof(r)){
		cerr << "sendrequest: write error\n";
		close(sfd);
		return -1;
	}

	close(sfd);
	return 0;
}

// return -1 if not found, -2 if ok, >=0 if error
#define HB_NOT_FOUND (-1)
#define HB_HANDLED (-2)
static int receiveHeartbeat(int sfd, HBCVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		if(v[i]->getFD() != sfd)
			continue;
		if(v[i]->readMessage() < 0)
			return (signed)i;
		return HB_HANDLED;
	}
	return HB_NOT_FOUND;
}

static int checkExpired(HBCVector &v){
	for(unsigned i = 0; i != v.size(); i++){
		if(v[i]->isExpired(HEARTBEAT_PERIOD * 2) == 0)
			continue;
		return (signed)i;
	}
	return -1;
}

static void deleteHBC(int i, HBCVector &v, FileSelector &fs){
	HeartbeatClient *hbc = v[i];
	fs.unregisterFD(v[i]->getFD());
	v.erase(v.begin() + i);
	delete hbc;
}

void changeAcceptor(HBCVector &acceptors, HBCVector &proposers,
const char *servicename, unsigned brokerport, FileSelector &fs,
const unsigned currentversion, const unsigned nacc, const char (*acc)[IP_LENGTH]){

	for(HBCVector::iterator i = acceptors.begin(); i != acceptors.end(); i++){
		fs.unregisterFD((*i)->getFD());
		delete (*i);
	}
	acceptors.clear();

	for(unsigned i = 0; i != nacc; i++){
		HeartbeatClient *hbc = new HeartbeatClient(acc[i]);
		fs.registerFD(hbc->getFD());
		acceptors.push_back(hbc);
	}
	for(unsigned i = 0; i != nacc; i++){
		if(sendRequest(servicename, acc[i], REQUEST_ACCEPTOR_PORT, brokerport,
		currentversion, acceptors) == -1){
			cerr << "error: send acceptor request\n";
		}
	}
	for(HBCVector::iterator i = proposers.begin(); i != proposers.end(); i++){
		char ip[IP_LENGTH];
		(*i)->getIP(ip);
		if(sendRequest(servicename, ip, REQUEST_PROPOSER_PORT, brokerport,
		currentversion, acceptors) == -1){
			cerr << "error: send proposer request\n";
		}
	}
}

// return 0 if committed
static int handleCommit(const enum PaxosResult r, ProposerStateMachine *(&psm), unsigned &currentversion,
HBCVector &acceptors, HBCVector &proposers,
const char *servicename, unsigned brokerport, FileSelector &fs, bool &ischangingacceptor){
	if(r != COMMITTING_SELF && r != COMMITTING_OTHER)
		return -1;

	Proposal *p = psm->getCommittingProposal();
	const enum ProposalType ptype = p->getType();

	if(currentversion + 1 != p->getVersion())
		cerr << "error:  committed&current version\n";
	currentversion = p->getVersion();

	if(ptype == ACCEPTORCHANGEPROPOSAL){
STDCOUT("requestor: committing: acceptorchange\n");
		AcceptorChangeProposal *ap = (AcceptorChangeProposal*)p;
		if(r != COMMITTING_SELF){
			cerr << "error: acceptor changed by others\n";
			return -1;
		}
		changeAcceptor(acceptors, proposers, servicename, brokerport, fs,
		currentversion, ap->getNumberOfAcceptors(), ap->getAcceptors());
		psm->sendCommitment(); // to old set

		ProposerStateMachine *newpsm = new ProposerStateMachine(
		fs, servicename, currentversion + 1,
		new AcceptorChangeProposal(currentversion,
		ap->getNumberOfAcceptors(), ap->getAcceptors()));
		delete psm; // also delete p;
		psm = newpsm;
		ischangingacceptor = false;
STDCOUT("requestor: committed: acceptorchange\n");
logTime("acceptorChangeCommitted");
	}
	if(ptype == RECOVERYPROPOSAL){
		cerr << "error: recovery proposal\n";
		return -1;
	}
	return 0;
}

// return -1 if socket error, 0 if ok
static int handlePaxosMessage(int ready, ProposerStateMachine *(&psm), unsigned &currentversion,
HBCVector &acceptors, HBCVector &proposers, const char *servicename, unsigned brokerport,
FileSelector &fs, bool &ischangingacceptor){
	PaxosMessage m;
	if(m.receive(ready) < 0)
		return -1;

	PaxosResult r = psm->handleMessage(ready, m);
	if(r == IGNORED){
		cerr << "error: ignore paxos message\n";
		return 0;
	}
	if(r == HANDLED)
		return 0;
	if(handleCommit(r, psm, currentversion, acceptors, proposers,
	servicename, brokerport, fs, ischangingacceptor) == -1)
		cerr << "error: handle commit\n";
	return 0;
}

static int readArguments(int readcount, int argc, const char *argv[],
const char**servicename, unsigned *brokerport, 
unsigned *expected_proposers, unsigned *expected_acceptors){
	switch(readcount){
	case 0:
		if(strlen(argv[0]) >= SERVICE_NAME_LENGTH){
			cerr << "cannot read argument: service name\n";
			return -1;
		}
		(*servicename) = argv[0];
		break;
	case 1:
		if(sscanf(argv[0], "%u", brokerport) != 1){
			cerr << "cannot read argument: broker port\n";
			return -1;
		}
		break;
	case 2:
		if(sscanf(argv[0], "%u", expected_proposers) != 1){
			cerr << "cannot read argument: number of proposers\n";
			return -1;
		}
		break;
	case 3:
		if(sscanf(argv[0], "%u", expected_acceptors) != 1){
			cerr << "cannot read argument: number of acceptors\n";
			return -1;
		}
		break;
	case 4:
		if(readProposerArgument(argc, (const char**)argv) != 0){
			cerr <<"cannot read argument: proposer list\n";
			return -1;
		}
		break;
	case 5:
		if(readAcceptorArgument(argc, (const char**)argv) != 0){
			cerr <<"cannot read argument: acceptor list\\n";
			return -1;
		}
		break;
	default:
		cerr << "too many arguments\n";
		return -1;
	}
	return 0;
}

int main(int argc,char *argv[]){
	replaceSIGPIPE();

	unsigned expected_proposers = 0;//MAX_PROPOSERS;
	unsigned expected_acceptors = 0;//MAX_ACCEPTORS;
	const char *landmarkip = NULL;
	const char *servicename; // in distributed mode
	unsigned brokerport;
	{// read arguments
		const int defaultargc = 6;
		const char *defaultargv[6] = {"", "5672", "0", "0", "paxoslist.txt", "paxoslist.txt"};
		int readcount = 0;
		for(int i = 1; i < argc; i++){
			if(strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--landmark") == 0){
				i++;
				if(i >= argc){
					cerr << "missing argument\n";
					return 0;
				}
				landmarkip = argv[i];
				continue;
			}
			if(readArguments(readcount, argc - i, (const char**)argv + i,
			&servicename, &brokerport,
			&expected_proposers, &expected_acceptors) != 0){
				return 0;
			}
			readcount++;
		}
		while(readcount < defaultargc){
			if(readArguments(readcount, defaultargc - readcount, defaultargv + readcount,
			&servicename, &brokerport,
			&expected_proposers, &expected_acceptors) != 0){
				return 0;
			}
			readcount++;
		}
		if(servicename[0] == '\0' && landmarkip == NULL){
			cerr << "need service name\n";
			return 0;
		}
		if(landmarkip != NULL){
			expected_proposers = expected_acceptors = 0;
		}
	}

	bool needchangeacceptor = (expected_acceptors == 0 ? false: true);
	bool ischangingacceptor = false;
	HBCVector proposers, acceptors;
	setLogName(servicename);
STDCOUT("requestor: " << servicename << "\n");

	int querysfd;
	vector<int> replysfd;
	FileSelector fs(0, 0);

	if(expected_acceptors > MAX_ACCEPTORS || expected_proposers > MAX_PROPOSERS){
		cerr << "assert: expected too large\n";
		return -1;
	}

	querysfd = tcpserversocket(QUERY_PROPOSER_PORT);
	fs.registerFD(querysfd);

	// add the landmark as a proposer
	if(landmarkip != NULL){
		HeartbeatClient *hbc = new HeartbeatClient(landmarkip);
		if(hbc->getFD() < 0){
			cerr << "cannot connect to landmark\n";
			return 0;
		}
		fs.registerFD(hbc->getFD());
		proposers.push_back(hbc);
	}

	// code for change acceptor
	unsigned currentversion = 0; // the last committed version
	ProposerStateMachine *psm = new ProposerStateMachine(fs, servicename, currentversion + 1,
	new AcceptorChangeProposal(currentversion, 0, NULL)); // version = 0, acceptor set = empty

	while(1){
		if(ischangingacceptor == false && needchangeacceptor == true
		/*acceptors.size() < expected_acceptors*/){
			needchangeacceptor = false;
			ischangingacceptor = true;
STDCOUT("requestor: add acceptors");
			char acc[MAX_ACCEPTORS][IP_LENGTH];
			for(unsigned i = 0; i != acceptors.size(); i++)
				acceptors[i]->getIP(acc[i]);
			for(unsigned i = acceptors.size(); i != expected_acceptors; i++){
				const char *accip = getAcceptorIP();
				if(accip == NULL){
					cerr << "error: insufficient acceptors\n";
					return 0;
				}
				strcpy(acc[i], accip);
STDCOUT(" " << accip);
			}
STDCOUT("\n");
STDCOUTFLUSH();
logTime("acceptorChangeProposal");
			PaxosResult r = psm->newProposal(
			new AcceptorChangeProposal(currentversion + 1, expected_acceptors, acc));

			handleCommit(r, psm, currentversion, acceptors, proposers,
			servicename, brokerport, fs, ischangingacceptor);
			continue;
		}
		if(proposers.size() < expected_proposers){// request

			const char *newip = getProposerIP();
			if(newip == NULL){
				cerr << "error: insufficient proposers\n";
				return 0;
			}
STDCOUT("requestor: add proposer " << newip << "\n");
STDCOUTFLUSH();
			if(sendRequest(servicename, newip, REQUEST_PROPOSER_PORT, brokerport,
			currentversion, acceptors) == -1){
				cerr << "error: send proposer request\n";
				continue;
			}
			HeartbeatClient *hbc = new HeartbeatClient(newip);
			if(hbc->getFD() >= 0){
				fs.registerFD(hbc->getFD());
			}
			proposers.push_back(hbc);
			// inform new proposer
STDCOUT("requestor: inform new proposers\n");
STDCOUTFLUSH();
			for(unsigned i = 0; i != replysfd.size(); i++){
				if(replyProposerList(replysfd[i], servicename, proposers) < 0){
					close(replysfd[i]);
					replysfd.erase(replysfd.begin() + i);
					i--;
				}
			}
logTime("requestProposer");
			continue;
		}

		int ready = fs.getReadReadyFD();

		if(ready == GET_READY_FD_TIMEOUT){// timeout, check heartbeat clients
			fs.resetTimeout(HEARTBEAT_PERIOD,0);
			int r;
			while((r = checkExpired(proposers)) >= 0){
STDCOUT("requestor: proposer heartbeat expired\n");
				deleteHBC(r, proposers, fs);
			}
			while((r = checkExpired(acceptors)) >= 0){
STDCOUT("requestor: acceptor heartbeat expired\n");
				deleteHBC(r, acceptors, fs);
			}
			continue;
		}
		if(ready == querysfd){
STDCOUT("requestor: get query\n");
			int newsfd = tcpaccept(querysfd, NULL);
			if(newsfd < 0)
				continue;
			if(replyProposerList(newsfd, servicename, proposers) < 0){
				close(newsfd);
				continue;
			}
			replysfd.push_back(newsfd);
			continue;
		}
		if(ready >= 0){// receive heartbeat or paxos
			int r;
			if((r = receiveHeartbeat(ready, proposers)) != HB_NOT_FOUND){
				if(r >= 0){
STDCOUT("requestor: lose proposer heartbeat " << proposers[r]->getIP() << "\n");
					deleteHBC(r , proposers, fs);
				}
				continue;
			}
			if((r = receiveHeartbeat(ready, acceptors)) != HB_NOT_FOUND){
				if(r >= 0){
STDCOUT("requestor: lose acceptor heartbeat " << acceptors[r]->getIP() << "\n");
					deleteHBC(r , acceptors, fs);
					needchangeacceptor = true;
				}
				continue;
			}
STDCOUT("requestor paxos msg\n");
			if(handlePaxosMessage(ready, psm, currentversion, acceptors, proposers,
			servicename, brokerport, fs, ischangingacceptor) == -1){
				fs.unregisterFD(ready);
			}
			continue;
		}
		// error
		cerr << "FileSelector error " << ready << "\n";
	}
	return 0;
}


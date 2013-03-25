#include<vector>
#include<iostream>
#include"socketlib.h"
#include"heartbeat_lib.h"
#include"discovery.h"
#include"fileselector.h"
#include"paxos.h"
#include<cstring>

typedef vector<HeartbeatClient*> HBCVector;

using namespace std;

static unsigned copyIPArray(char (*ip)[IP_LENGTH], HBCVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		v[i]->getIP(ip[i]);
	}
	return v.size();
}

static void replyProposerList(int replysfd, HBCVector &proposers){
	struct ReplyProposer r;
	memset(&r, 0, sizeof(r));
	r.length = copyIPArray(r.ip, proposers);

	if(write(replysfd, &r, sizeof(r)) != sizeof(r))
		cerr << "replyProposerList: write error\n";
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

int receiveHeartbeat(int sfd, HBCVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		if(v[i]->getFD() != sfd)
			continue;
		v[i]->readMessage();
		return (signed)i;
	}
	return -1;
}

static HeartbeatClient *checkExpired(HBCVector &v, FileSelector &fs){
	HeartbeatClient *r = NULL;
	for(HBCVector::iterator i = v.begin(); i != v.end(); i++){
		if((*i)->isExpired(HEARTBEAT_PERIOD * 2) == 0)
			continue;
		r = (*i);
		fs.unregisterFD((*i)->getFD());
		v.erase(i);
	}
	return r;
}

void changeAcceptor(HBCVector &acceptors,
const char *servicename, unsigned brokerport, FileSelector &fs,
const unsigned currentversion, const unsigned nacc, const char (*acc)[IP_LENGTH]){

	for(HBCVector::iterator i = acceptors.begin(); i != acceptors.end(); i++){
		fs.unregisterFD((*i)->getFD());
		delete (*i);
	}
	acceptors.clear();

	for(unsigned i; i != nacc; i++){
		HeartbeatClient *hbc = new HeartbeatClient(acc[i]);
		fs.registerFD(hbc->getFD());
		acceptors.push_back(hbc);
	}
	for(unsigned i; i != nacc; i++){
		if(sendRequest(servicename, acc[i], REQUEST_ACCEPTOR_PORT, brokerport,
		currentversion, acceptors) == -1){
			cerr << "error: send acceptor request\n";
		}
	}
}

static int handleCommit(const enum PaxosResult r, ProposerStateMachine *(&psm), unsigned &currentversion,
HBCVector &acceptors, const char *servicename, unsigned brokerport, FileSelector &fs){
	if(r != COMMITTING_SELF && r != COMMITTING_OTHER)
		return -1;

	Proposal *p = psm->getCommittingProposal();
	const enum ProposalType ptype = p->getType();

	if(currentversion + 1 != p->getVersion())
		cerr << "error:  committed&current version\n";
	currentversion = p->getVersion();

	if(ptype == ACCEPTORCHANGEPROPOSAL){
		AcceptorChangeProposal *ap = (AcceptorChangeProposal*)p;
		if(r != COMMITTING_SELF){
			cerr << "error: acceptor changed by others\n";
			return -1;
		}
		changeAcceptor(acceptors, servicename, brokerport, fs,
		currentversion + 1, ap->getNumberOfAcceptors(), ap->getAcceptors());
		psm->sendCommitment(); // to old set

		ProposerStateMachine *newpsm = new ProposerStateMachine(
		fs, servicename, currentversion + 1,
		new AcceptorChangeProposal(currentversion,
		ap->getNumberOfAcceptors(), ap->getAcceptors()));
		delete psm; // also delete p;
		psm = newpsm;
	}
	if(ptype == RECOVERYPROPOSAL){
		cerr << "error: recovery proposal\n";
		return -1;
	}
	return 0;
}

// return -1 if socket error, 0 if ok
static int handlePaxosMessage(int ready, ProposerStateMachine *(&psm), unsigned &currentversion,
HBCVector &acceptors, const char *servicename, unsigned brokerport, FileSelector &fs){
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
	if(handleCommit(r, psm, currentversion, acceptors, servicename, brokerport, fs) == -1)
		cerr << "error: handle commit\n";
	return 0;
}

int main(int argc,char *argv[]){
	replaceSIGPIPE();

	const unsigned expected_proposers = MAX_PROPOSERS;
	const unsigned expected_acceptors = MAX_ACCEPTORS;
	HBCVector proposers, acceptors;
	const char *servicename = argv[1];
	const unsigned brokerport = 5672;
	int querysfd;
	FileSelector fs(0, 0);

	if(expected_acceptors > MAX_ACCEPTORS || expected_proposers > MAX_PROPOSERS){
		cout << "assert: expected too large\n";
		cerr << "assert: expected too large\n";
		return -1;
	}
	if(argc < 2){
		cout << "need service name\n";
		cerr << "need service name\n";
		return -1;
	}

	querysfd = tcpserversocket(QUERY_PROPOSER_PORT);
	fs.registerFD(querysfd);

	unsigned currentversion = 0; // the last committed version
	ProposerStateMachine *psm = new ProposerStateMachine(fs, servicename, currentversion + 1,
	new AcceptorChangeProposal(currentversion, 0, NULL)); // version = 0, acceptor set = empty
	while(1){
		if(acceptors.size() < expected_acceptors){
			char acc[MAX_ACCEPTORS][IP_LENGTH];
			for(unsigned i = 0; i != acceptors.size(); i++)
				acceptors[i]->getIP(acc[i]);
			for(unsigned i = acceptors.size(); i != expected_acceptors; i++)
				strcpy(acc[i], discoverAcceptor());

			PaxosResult r = psm->newProposal(
			new AcceptorChangeProposal(currentversion + 1, expected_acceptors, acc));

			handleCommit(r, psm, currentversion, acceptors,
			servicename, brokerport, fs);
			
		}
		if(proposers.size() < expected_proposers){// request
			const char *newip = discoverProposer();
			if(sendRequest(servicename, newip, REQUEST_PROPOSER_PORT, brokerport,
			currentversion, acceptors) == -1){
				cerr << "error: send proposer request\n";
				continue;
			}
			HeartbeatClient *hbc = new HeartbeatClient(newip);
			fs.registerFD(hbc->getFD());
			proposers.push_back(hbc);
			continue;
		}

		int ready = fs.getReadReadyFD();

		if(ready == GET_READY_FD_TIMEOUT){// timeout, check heartbeat clients
			fs.resetTimeout(HEARTBEAT_PERIOD,0);
			HeartbeatClient *hbc;
			while((hbc = checkExpired(proposers, fs)) != NULL){
				delete hbc;
			}
			while((hbc = checkExpired(acceptors, fs)) != NULL){
				delete hbc;
			}
			continue;
		}
		if(ready == querysfd){
			int replysfd = tcpaccept(querysfd, NULL);
			if(replysfd >= 0)
				replyProposerList(replysfd, proposers);
			close(replysfd);
			continue;
		}
		if(ready >= 0){// receive heartbeat or paxos
			if(receiveHeartbeat(ready, proposers) != -1)
				continue;
			if(receiveHeartbeat(ready, acceptors) != -1)
				continue;
			if(handlePaxosMessage(ready, psm, currentversion, acceptors,
			servicename, brokerport, fs) == -1){
				fs.unregisterFD(ready);
			}
			continue;
		}
		// error
		cerr << "FileSelector error " << ready << "\n";
	}
	return 0;
}

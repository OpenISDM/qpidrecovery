#include<vector>
#include<iostream>
#include"socketlib.h"
#include"heartbeat_lib.h"
#include"discovery.h"
#include"fileselector.h"
#include<cstring>

typedef vector<HeartbeatClient*> HBCVector;

using namespace std;

const unsigned expected_proposers = MAX_PROPOSERS;
const unsigned expected_acceptors = MAX_ACCEPTORS;

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

static HeartbeatClient *sendRequest(const char *servicename, string ip, unsigned port,unsigned brokerport,
unsigned version, HBCVector &acceptors, FileSelector &fs){
	struct ParticipateRequest r;
	memset(&r, 0, sizeof(r));
	strcpy(r.name, servicename);
	r.version = version;
	r.length = copyIPArray(r.acceptor, acceptors);
	r.brokerport = brokerport;

	int sfd = tcpconnect(ip.c_str(), port);
	if(sfd < 0)
		return NULL;
	if(write(sfd, &r, sizeof(r)) != sizeof(r)){
		cerr << "sendrequest: write error\n";
		close(sfd);
		return NULL;
	}

	close(sfd);
	HeartbeatClient *hbc = new HeartbeatClient(ip.c_str());
	fs.registerFD(hbc->getFD());
	return hbc;
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

int main(int argc,char *argv[]){
	replaceSIGPIPE();

	vector<HeartbeatClient*> proposers, acceptors;
	const char *servicename = argv[1];
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
	while(1){

		if(acceptors.size() < expected_acceptors){
			// TODO: propose & request
		}
		if(proposers.size() < expected_proposers){// request
			HeartbeatClient *hbc = sendRequest(servicename,
			discoverIP(), REQUEST_PROPOSER_PORT,
			5672, 1, acceptors, fs);
			if(hbc == NULL){
				cerr << "error: send proposer request\n";
				continue;
			}
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
		if(ready >= 0){// receive heartbeat or TODO: paxos
			if(
			receiveHeartbeat(ready, proposers) == -1 &&
			receiveHeartbeat(ready, acceptors) == -1
			)
				cerr << "error: cannot find HeartbeatClient\n";
			continue;
		}
		// error
		cerr << "FileSelector error " << ready << "\n";
	}
	return 0;
}

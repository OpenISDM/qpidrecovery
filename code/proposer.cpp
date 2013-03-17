#include<string>
#include<iostream>
#include<vector>
#include<cstring>

#include"common.h"
#include"fileselector.h"
#include"recoverymanager.h"
#include"paxos.h"
#include"memoryusage.h"
#include"socketlib.h"
#include"heartbeat_lib.h"
#include"discovery.h"

using namespace std;

//typedef vector<struct QueryAddress> QAVector;

class Monitored{
public:
	ProposerStateMachine *psm;
	HeartbeatClient *hbc;
	char name[SERVICE_NAME_LENGTH];
	char ip[IP_LENGTH];
	unsigned port;

	Monitored(ProposerStateMachine *proposer, HeartbeatClient *heartbeat,
	const char *servicename, const char *brokerip, unsigned brokerport);
};
typedef vector<Monitored> MVector;

Monitored::Monitored(ProposerStateMachine *proposer, HeartbeatClient *heartbeat,
const char *servicename, const char *brokerip, unsigned brokerport){
	this->psm = proposer;
	this->hbc = heartbeat;
	strcpy(this->name, servicename);
	strcpy(this->ip, brokerip);
	this->port = brokerport;
}

void createProposer(const char *name, unsigned ver, unsigned nacc, const char (*acc)[IP_LENGTH],
MVector &monitor, const char *from, unsigned brokerport, FileSelector &fs){

	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		if((*i).psm->isRecepient(name) != 0){
			delete (*i).psm;
			if((*i).hbc != NULL){
				fs.unregisterFD((*i).hbc->getFD());
				delete (*i).hbc;
			}
			monitor.erase(i);
			break;
		}
	}
	HeartbeatClient *hbc = new HeartbeatClient(from);
	fs.registerFD(hbc->getFD());

	Monitored mon(
	new ProposerStateMachine(fs, name, ver, nacc, acc),
	hbc,
	name, from, brokerport);
	monitor.push_back(mon);
}

int searchServiceByName(MVector &monitor, const char *searchname){
	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		if(strcmp((*i).name, searchname) != 0)
			continue;
		return (i - monitor.begin());
	}
	return -1;
}



// return -1 if socket error, 0 if ok
int handlePaxosMessage(int ready, MVector &monitor, RecoveryManager &recovery){
	PaxosMessage m;
	if(m.receive(ready) < 0)
		return -1;

	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		Monitored &mon = (*i);
		ProposerStateMachine *psm = mon.psm;
		PaxosResult r = psm->handleMessage(ready, m);
		if(r == IGNORED)
			continue;
		if(r == HANDLED)
			return 0;
		if(r != COMMITTING){
			cerr << "unknown paxos result\n";
			continue;
		}
		Proposal *p = psm->getCommittingProposal();
		if(p->getType() != RECOVERYPROPOSAL)
			cerr << "error: commit\n";
		{
			RecoveryProposal *rp = (RecoveryProposal*)p;
			//recovery.do(
			//rp->getFailIP(), rp->getFailPort(),
			//rp->getBackupIP(), rp->getBackupPort())

			strcpy(mon.ip, rp->getBackupIP());
			mon.port = rp->getBackupPort();

			psm->sendCommitment();
		}
		/*
		case ACCEPTORCHANGEPROPOSAL:
			{
				AcceptorChangeProposal *rp = (AcceptorChangeProposal*)p;
				plist.push_back(new ProposerStateMachine(
				fs, m.header.name, rp->getVersion(),
				rp->getNumberOfAcceptors(), rp->getAcceptors())
				);
			}
			break;
		*/
		return 0;
	}
	cerr << "msg unhandled\n";
	return 0;
}

int receiveHeartbeat(int sfd, MVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		if(v[i].hbc->getFD() != sfd)
			continue;
		v[i].hbc->readMessage();
		return (signed)i;
	}
	return -1;
}

int main(int argc, char *argv[]){
	replaceSIGPIPE();

	int requestsfd, querysfd;
	FileSelector fs(0,0);
	MVector monitor;
	RecoveryManager recovery;

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
				createProposer(r.name, r.version, r.length, r.acceptor,
				monitor, from, r.brokerport, fs);

				recovery.addBroker(from, r.brokerport);
			}
			continue;
		}
		if(ready == querysfd){
			char from[IP_LENGTH];
			struct QueryAddress r;
			int newsfd;
			newsfd = acceptRead<struct QueryAddress>(querysfd, from, r);
			if(newsfd >= 0){
				if(searchServiceByName(monitor, r.name) != -1)
					write(newsfd, &r, sizeof(struct QueryAddress));
				close(newsfd);
			}
			continue;
		}
		if(receiveHeartbeat(ready, monitor) != -1){
			continue;
		}
		if(ready >= 0){
			if(handlePaxosMessage(ready, monitor, recovery) == -1)
				fs.unregisterFD(ready);
			continue;
		}
		if(ready == GET_READY_FD_TIMEOUT){
			fs.resetTimeout(4,0);
			for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
				Monitored &mon = *i;
				mon.psm->checkTimeout(4); // proposer state machine

				if(mon.hbc == NULL) // heartbeat client
					continue;
				if(mon.hbc->isExpired(HEARTBEAT_PERIOD * 2) == 0)
					continue;
				fs.unregisterFD((mon.hbc)->getFD());
				delete mon.hbc;
				mon.hbc = NULL;
				mon.psm->newProposal(
				new RecoveryProposal(mon.ip, mon.port, discoverIP().c_str(), 5672, 100)
				//TODO: discover score, port
				);
			}
			continue;
		}
	}
}

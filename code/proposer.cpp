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

class Monitored{
public:
	ProposerStateMachine *psm;
	HeartbeatClient *hbc;
	char name[SERVICE_NAME_LENGTH];
	char ip[IP_LENGTH];
	unsigned port;

	Monitored(ProposerStateMachine *proposer, HeartbeatClient *heartbeat,
	const char *servicename, const char *brokerip, unsigned brokerport){
		this->psm = proposer;
		this->hbc = heartbeat;
		strcpy(this->name, servicename);
		strcpy(this->ip, brokerip);
		this->port = brokerport;
	}
};
typedef vector<Monitored> MVector;

static void createProposer(const char *name, unsigned votingver,
unsigned committedver, unsigned nacc, const char (*acc)[IP_LENGTH],
MVector &monitor, const char *from, unsigned brokerport, FileSelector &fs){

	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		if(strcmp((*i).name, name) == 0){
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
	new ProposerStateMachine(fs, name, votingver,
	new AcceptorChangeProposal(committedver, nacc, acc)),
	hbc, name, from, brokerport);
	monitor.push_back(mon);
}

static int searchServiceByName(MVector &monitor, const char *searchname){
	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		if(strcmp((*i).name, searchname) != 0)
			continue;
		return (i - monitor.begin());
	}
	return -1;
}

static int handleCommit(const enum PaxosResult r, Monitored &mon, FileSelector &fs,
RecoveryManager &recovery){
	if(r != COMMITTING_SELF && r != COMMITTING_OTHER)
		return -1;

	Proposal *p = mon.psm->getCommittingProposal();
	const enum ProposalType ptype = p->getType();
	if(ptype == RECOVERYPROPOSAL){
		RecoveryProposal *rp = (RecoveryProposal*)p;
		strcpy(mon.ip, rp->getBackupIP());
		mon.port = rp->getBackupPort();
		if(r == COMMITTING_SELF){
			recovery.copyObjects(
			rp->getFailIP(), rp->getFailPort(),
			rp->getBackupIP(), rp->getBackupPort());
			mon.psm->sendCommitment();
		}
	}
	if(ptype == ACCEPTORCHANGEPROPOSAL){
		AcceptorChangeProposal *ap = (AcceptorChangeProposal*)p;
		ProposerStateMachine *newpsm = new ProposerStateMachine(
		fs, mon.name, ap->getVersion() + 1,
		new AcceptorChangeProposal(ap->getVersion(),
		ap->getNumberOfAcceptors(), ap->getAcceptors()));
		delete mon.psm; // also delete p
		mon.psm = newpsm;
	}
	return 0;
}

// return -1 if socket error, 0 if ok
static int handlePaxosMessage(int ready, MVector &monitor, RecoveryManager &recovery, FileSelector &fs){
	PaxosMessage m;
	if(m.receive(ready) < 0)
		return -1;

	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		Monitored &mon = (*i);
		ProposerStateMachine *psm = mon.psm;
		const PaxosResult r = psm->handleMessage(ready, m);
		if(r == IGNORED)
			continue;
		if(r == HANDLED)
			return 0;
		if(handleCommit(r, mon, fs, recovery) == 0)
			return 0;
		cerr << "error: unknown message";
	}
	cerr << "msg unhandled\n";
	return 0;
}

static int receiveHeartbeat(int sfd, MVector &v){
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

	int requestsfd, querysfd, recoveryfd;
	FileSelector fs(0, 0);
	MVector monitor;
	RecoveryManager recovery;

	requestsfd = tcpserversocket(REQUEST_PROPOSER_PORT);
	querysfd = tcpserversocket(QUERY_BACKUP_PORT);
	recoveryfd = recovery.getEventFD();

	fs.registerFD(recoveryfd);
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
				createProposer(r.name, r.votingversion,
				r.committedversion, r.length, r.acceptor,
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
		if(ready == recoveryfd){
			ListenerEvent *le = recovery.handleEvent();
			if(le->getType() == BROKERDISCONNECTION){
				BrokerDisconnectionEvent *bde = (BrokerDisconnectionEvent*)le;
				recovery.deleteBroker(bde->brokerip, bde->brokerport);
			}
			delete le;
			continue;
		}

		if(ready >= 0){
			if(receiveHeartbeat(ready, monitor) != -1){
				continue;
			}
			if(handlePaxosMessage(ready, monitor, recovery, fs) == -1)
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
				recovery.deleteBroker(mon.ip, mon.port);

				PaxosResult r = mon.psm->newProposal(
				new RecoveryProposal(mon.psm->getVotingVersion(),
				mon.ip, mon.port, discoverBackup(), 5672, 50) //TODO: discover score, port
				);
				handleCommit(r, mon, fs, recovery);
			}
			continue;
		}
	}
}

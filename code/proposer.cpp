#include<string>
#include<iostream>
#include<vector>
#include<cstring>
#include<cstdio>

#include"common.h"
#include"fileselector.h"
#include"recoverymanager.h"
#include"proposer_link.h"
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
	vector<int> informlist; //socket, inform when ip:port changed

private:
	int inform(int sfd){
		const int rasize = sizeof(struct ReplyAddress);
		struct ReplyAddress r;
		strcpy(r.name, this->name);
		strcpy(r.ip, this->ip);
		r.port = this->port;
		if(write(sfd, &r, rasize) != rasize)
			return -1;
		return 0;
	}
public:
	Monitored(ProposerStateMachine *proposer, HeartbeatClient *heartbeat,
	const char *servicename, const char *brokerip, unsigned brokerport){
		this->psm = proposer;
		this->hbc = heartbeat;
		strcpy(this->name, servicename);
		strcpy(this->ip, brokerip);
		this->port = brokerport;
		informlist.clear();
	}

	void addInformList(int newsfd){
		if(this->inform(newsfd) < 0){
			close(newsfd);
			return;
		}
		informlist.push_back(newsfd);
	}

	void informAll(){
		for(unsigned i = 0; i != this->informlist.size(); i++){
			if(this->inform(informlist[i]) < 0){
				close(this->informlist[i]);
				this->informlist.erase(this->informlist.begin() + i);
				i--;
			}
		}
	}
};
typedef vector<Monitored> MVector;

struct ProposerList{
	char ip[IP_LENGTH];
	unsigned port;
	int sfd;
	struct ReplyProposer plist;
};
typedef vector<struct ProposerList> PVector;

static struct ProposerList *searchProposerListByIPPort(PVector &dstproposers,
const char *searchip, unsigned searchport){
	for(PVector::iterator i = dstproposers.begin(); i != dstproposers.end(); i++){
		if(strcmp((*i).ip, searchip) == 0 && (*i).port == searchport)
			return &(*i);
	}
	return NULL;
}

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

static Monitored *searchServiceByName(MVector &monitor, const char *searchname){
	for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
		if(strcmp((*i).name, searchname) == 0)
			return &(*i);
	}
	return NULL;
}

static int handleCommit(const enum PaxosResult r, Monitored &mon, FileSelector &fs,
RecoveryManager &recovery){
	if(r != COMMITTING_SELF && r != COMMITTING_OTHER)
		return -1;
STDCOUT("commit at " << getSecond() << "\n");
	Proposal *p = mon.psm->getCommittingProposal();
	const enum ProposalType ptype = p->getType();
	if(ptype == RECOVERYPROPOSAL){
STDCOUT("recovery proposal\n");
		// change Monitored
		RecoveryProposal *rp = (RecoveryProposal*)p;
		strcpy(mon.ip, rp->getBackupIP());
		mon.port = rp->getBackupPort();
		if(r == COMMITTING_SELF){
STDCOUT("recovery begins at " << getSecond() << " \n");
			recovery.copyObjects(
			rp->getFailIP(), rp->getFailPort(),
			rp->getBackupIP(), rp->getBackupPort());
			mon.psm->sendCommitment();
STDCOUT("recovery ends at " << getSecond() << "\n");
		}
		mon.informAll();
STDCOUT("inform all\n");
	}
	if(ptype == ACCEPTORCHANGEPROPOSAL){
STDCOUT("acceptor change\n");
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
STDCOUT("pmessage\n");
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

static void startRecovery(Monitored &mon, FileSelector &fs, RecoveryManager &recovery){
STDCOUT("lose heartbeat: " << mon.ip << ":" << mon.port << " " << mon.name << "\n");
	fs.unregisterFD((mon.hbc)->getFD());
	delete mon.hbc;
	mon.hbc = NULL;
	recovery.deleteBroker(mon.ip, mon.port);
STDCOUT("recovery proposal at " << getSecond() << "\n");
	unsigned backupport;
	const char *backupip = discoverBackup(backupport);
	PaxosResult r = mon.psm->newProposal(
	new RecoveryProposal(mon.psm->getVotingVersion(),
	mon.ip, mon.port, backupip, backupport, 50)
	);
	handleCommit(r, mon, fs, recovery);
}

static int listenProposerList(PVector &dstproposers, char* dstip, unsigned dstport, FileSelector &fs){
	struct ProposerList pl;
	memset(&pl, 0 ,sizeof(struct ProposerList));
	strcpy(pl.ip, dstip);
	pl.port = dstport;
	pl.sfd = tcpconnect(dstip, QUERY_PROPOSER_PORT);
	if(pl.sfd < 0){
		cerr << "error: listening proposer list\n";
		return -1;
	}
	fs.registerFD(pl.sfd);
	dstproposers.push_back(pl);
	return 0;
}

// return 1 if handled, -1 if error, 0 if not handled
static int readProposerList(int ready, PVector &dstproposers, FileSelector &fs){
	for(PVector::iterator i = dstproposers.begin(); i != dstproposers.end(); i++){
		if((*i).sfd != ready)
			continue;

		const int rpsize = sizeof(ReplyProposer);
		int r = read(ready, &((*i).plist), rpsize);
		if(r == rpsize){
			return 1;
		}
		// if error
		fs.unregisterFD(ready);
		close(ready);
		dstproposers.erase(i);
		return -1;
	}
	return 0;
}

// return -1 if not found, -2 if ok, >=0 if error
#define HB_NOT_FOUND (-1)
#define HB_HANDLED (-2)
static int receiveHeartbeat(int sfd, MVector &v){
	for(unsigned i = 0; i < v.size(); i++){
		if(v[i].hbc == NULL)
			continue;
		if(v[i].hbc->getFD() != sfd)
			continue;
		if(v[i].hbc->readMessage() < 0)
			return (signed)i;
		return HB_HANDLED;
	}
	return HB_NOT_FOUND;
}

int main(int argc, char *argv[]){
	replaceSIGPIPE();

	int requestsfd, querysfd, recoveryfd;
	FileSelector fs(0, 0);
	MVector monitor;
	PVector dstproposers;
	LinkDstListener dstlistener(fs);
	RecoveryManager recovery;

	requestsfd = tcpserversocket(REQUEST_PROPOSER_PORT);
	querysfd = tcpserversocket(QUERY_BACKUP_PORT);
	recoveryfd = recovery.getEventFD();

	fs.registerFD(recoveryfd);
	fs.registerFD(requestsfd);
	fs.registerFD(querysfd);
	while(1){
DELAY();
STDCOUT(".");
STDCOUTFLUSH();
		int ready = fs.getReadReadyFD();

		if(ready == requestsfd){
STDCOUT(ready << "(requestsfd)\n");
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
				// subscribe proposer list from newly added link destination
				string fromurl = IPPortToUrl(from, r.brokerport);
				for(int i = recovery.firstLinkInfo(fromurl);
				i >= 0;
				i = recovery.nextLinkInfo(fromurl, i)){
					char dstip[IP_LENGTH];
					unsigned dstport;
					urlToIPPort(recovery.getLinkDst(i), dstip,dstport);
					listenProposerList(dstproposers, dstip, dstport, fs);
				}
			}
			continue;
		}
		if(ready == querysfd){
STDCOUT(ready << " (querysfd)\n");
			char from[IP_LENGTH];
			struct ReplyAddress r;
			int newsfd;
			newsfd = acceptRead<struct ReplyAddress>(querysfd, from, r);
			if(newsfd >= 0){
				Monitored *m = searchServiceByName(monitor, r.name);
				if(m != NULL)
					m->addInformList(newsfd);
				else
					close(newsfd);
			}
			continue;
		}
		if(ready == recoveryfd){
STDCOUT(ready << "(recoverysfd)\n");
			ListenerEvent *le = recovery.handleEvent();
			if(le->getType() == BROKERDISCONNECTION){
				BrokerDisconnectionEvent *bde = (BrokerDisconnectionEvent*)le;
				recovery.deleteBroker(bde->brokerip, bde->brokerport);
			}
			if(le->getType() == LINKDOWN){
				LinkDownEvent *lde = (LinkDownEvent*)le;
				struct ProposerList *p =
				searchProposerListByIPPort(dstproposers, lde->dstip, lde->dstport);
				if(p != NULL)
					dstlistener.listenAddressChange(p->plist,
					lde->srcip, lde->srcport, lde->dstip, lde->dstport);
				else
					cerr << "unknown link dest\n";
			}
			delete le;
			continue;
		}

		if(ready >= 0){
			{
				int r = receiveHeartbeat(ready, monitor);
				if(r != HB_NOT_FOUND){
					if(r >= 0){ // read ready fd error
						startRecovery(monitor[r], fs, recovery);
					}
					continue;
				}
			}
			{
				char newip[IP_LENGTH], oldip[IP_LENGTH], srcip[IP_LENGTH];
				unsigned newport, oldport, srcport;
				int r = dstlistener.readAddressChange(ready,
				srcip, srcport, oldip, oldport, newip, newport);
				if(r != 0){
					if(r != 1)
						continue;
					recovery.reroute(srcip, srcport,
					oldip, oldport,	newip, newport);
					continue;
				}
			}
			if(readProposerList(ready, dstproposers, fs) != 0)
				continue;
			if(handlePaxosMessage(ready, monitor, recovery, fs) == -1)
				fs.unregisterFD(ready);
			continue;
		}
		if(ready == GET_READY_FD_TIMEOUT){
			static unsigned totaltime = 0;
			totaltime+=4;
			if(totaltime > 60) // 120 seconds
				totaltime = 0;

			fs.resetTimeout(4,0);
			for(MVector::iterator i = monitor.begin(); i != monitor.end(); i++){
				Monitored &mon = *i;
				mon.psm->checkTimeout(4); // proposer state machine

				if(totaltime == 0)
					mon.informAll(); // check disconnection

				if(mon.hbc == NULL) // proposing or recovered
					continue;
				if(mon.hbc->isExpired(HEARTBEAT_PERIOD * 2) == 0) // no heartbeat
					continue;
				startRecovery(mon, fs, recovery);
			}
			continue;
		}
	}
}

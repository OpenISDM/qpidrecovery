#include<cstring>
#include"socketlib.h"
#include"paxos.h"
#include"timestamp.h"
#include<unistd.h>

int receivePaxosHeader(int sfd, struct PaxosHeader &ph);
enum ProposalType receiveProposalType(int sfd);
Proposal *receiveProposal(int sfd);

int sendPaxosHeader(int sfd, enum PaxosMessageType t, unsigned v, const char *m);
int sendHeaderProposal(int sfd,
enum PaxosMessageType t, unsigned v, const char *m, Proposal *p);

// 8 send/receive functions
// receivePaxosHeader
// receiveProposal = receiveProposalType + sendReceiveContent
// sendHeaderProposal = sendPaxosHeader + sendTypeContent
// sendTypeContent = sendType + sendReceiveContent

// struct PaxosHeader
int sendHeaderProposal(int sfd,
enum PaxosMessageType t, unsigned v, const char *m, Proposal *p){
	int rh, rp;
	rh = sendPaxosHeader(sfd, t, v, m);
	if(rh < 0)
		return -1;
	rp = p->sendTypeContent(sfd);
	if(rp < 0)
		return -1;
	return rh + rp;
}


int sendPaxosHeader(int sfd, enum PaxosMessageType t, unsigned v, const char *m){
	const int sph = sizeof(struct PaxosHeader);
	int r;
	struct PaxosHeader ph;
	ph.type = t;
	ph.version = v;
	strcpy(ph.name, m);
	r = write(sfd, &ph, sph);
	return r == sph? r: -1;
}

int receivePaxosHeader(int sfd, struct PaxosHeader &ph){
	const int sph = sizeof(struct PaxosHeader);
	return read(sfd, &ph, sph) == sph? 0: -1;
}

enum ProposalType receiveProposalType(int sfd){
	const int spt = sizeof(enum ProposalType);
	enum ProposalType t;
	int r;
	r = read(sfd, &t, spt);
	return r == spt? t: NOPROPOSAL;
}

Proposal *receiveProposal(int sfd){
	enum ProposalType t;
	Proposal *p;
	t = receiveProposalType(sfd);
	switch(t){
	case NOPROPOSAL:
		return NULL;
	case RECOVERYPROPOSAL:
		p = new RecoveryProposal();
		break;
	case ACCEPTORCHANGEPROPOSAL:
		p = new AcceptorChangeProposal();
		break;
	}
	if(p->sendReceiveContent('r', sfd) < 0){
		delete p;
		return NULL;
	}
	return p;
}

// class PaxosMessage
PaxosMessage::PaxosMessage(){
	memset(&(this->header), 0, sizeof(struct PaxosHeader));
	proposal = NULL;
}

PaxosMessage::~PaxosMessage(){
	if(proposal != NULL)
		delete proposal;
}

int PaxosMessage::receive(int sfd){
	int rh;
	rh = receivePaxosHeader(sfd, this->header);
	if(rh < 0)
		return -1;
	this->proposal = receiveProposal(sfd);
	if(this->proposal == NULL)
		return -1;
	return rh;
}

Proposal *PaxosMessage::getProposalPtr(){
	Proposal *p;
	p = this->proposal;
	this->proposal = NULL;
	return p;
}

// state machine
PaxosStateMachine::PaxosStateMachine(const char *m){
	strcpy(this->name, m);
}

int PaxosStateMachine::isRecepient(const char *m){
	return strcmp(m, this->name) == 0;
}

const char *PaxosStateMachine::getName(){
	return this->name;
}

// class AcceptorStateMachine
void AcceptorStateMachine::initialize(unsigned v, unsigned n, const char (*a)[IP_LENGTH]){
	this->version = v;
	this->nacceptor = n;
	for(unsigned i = 0; i != n ;i++)
		strcpy(this->acceptors[i], a[i]);
	this->promise = new NoProposal();
	this->state = PHASE1;
	this->timestamp = getSecond();
}

AcceptorStateMachine::AcceptorStateMachine(const char *m, unsigned v,
unsigned n, const char (*a)[IP_LENGTH]):
PaxosStateMachine(m){
	this->initialize(v, n, a);
}

AcceptorStateMachine::~AcceptorStateMachine(){
	delete this->promise;
}

enum PaxosResult AcceptorStateMachine::checkTimeout(double timeout){
	if(getSecond() - this->timestamp <= timeout)
		return IGNORED;

	switch(this->state){
	case INITIAL:
	case PHASE3:
		return IGNORED;
	case PHASE1:
	case PHASE2:
		delete this->promise;
		this->promise = new NoProposal();
		this->state = PHASE1;
		return HANDLED;
	}
	return IGNORED;
}

enum PaxosResult AcceptorStateMachine::handlePhase1Message(int replysfd, Proposal *p, unsigned v){
	if(this->state != PHASE1)
		return HANDLED;

	enum PaxosMessageType reply;
	if(this->promise->getValue() > p->getValue()){
		delete p;
		reply = PHASE1NACK;
	}
	else{
		delete this->promise;
		this->promise = p;
		reply = PHASE1ACK;
		this->timestamp = getSecond();
	}
	sendPaxosHeader(replysfd, reply, v, this->getName());
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handlePhase2Message(int replysfd, Proposal *p, unsigned v){
	if(this->state != PHASE1)
		return HANDLED;

	enum PaxosMessageType reply;
	if(this->promise->isEqual(*p) == false){
		reply = PHASE2NACK;
	}
	else{
		reply = PHASE2ACK;
		this->timestamp = getSecond();
		this->state =PHASE2;
	}
	sendPaxosHeader(replysfd, reply, v, this->getName());
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handlePhase3Message(Proposal *p){
	delete this->promise;
	if(p->getType() == ACCEPTORCHANGEPROPOSAL){
		AcceptorChangeProposal *rp = (AcceptorChangeProposal*)p;
		this->initialize(rp->getVersion(),
		rp->getNumberOfAcceptors(), rp->getAcceptors());
		delete p;
	}
	else{
		this->promise = p;
		this->state = PHASE3;
		this->timestamp = getSecond();
	}
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handleMessage(int replysfd, PaxosMessage &m){
	struct PaxosHeader &ph = m.header;

	if(isRecepient(ph.name) == 0)
		return IGNORED;
	/*
	if(this->version < ph.version)
		error occurred when changing acceptor set but still reply the proposer
	*/

	if(this->version > ph.version){ // reply the result of ph.version
		if(ph.type != PHASE3COMMIT){
			Proposal* ac = new AcceptorChangeProposal(this->version,
			this->nacceptor, this->acceptors);
			sendHeaderProposal(replysfd, PHASE3COMMIT, ph.version, ph.name, ac);
			delete ac;
		}
		return HANDLED;
	}
	if(this->state == PHASE3){ // reply the result of this->version
		if(ph.type != PHASE3COMMIT)
			sendHeaderProposal(replysfd, PHASE3COMMIT,
			this->version, this->getName(), this->promise);
		return HANDLED;
	}

	switch(ph.type){
	case PHASE1PROPOSAL:
		return this->handlePhase1Message(replysfd, m.getProposalPtr(), ph.version);
	case PHASE2REQUEST:
		return this->handlePhase2Message(replysfd, m.getProposalPtr(), ph.version);
	case PHASE3COMMIT:
		return this->handlePhase3Message(m.getProposalPtr());
	default:
		return HANDLED;
	}
	return HANDLED;
}

// class ProposerStateMachine
void ProposerStateMachine::initialState(){
	this->phase1ack = this->phase1nack = 0;
	this->phase2ack = this->phase1nack = 0;
	this->state = INITIAL;
	this->timestamp = getSecond();
}

ProposerStateMachine::ProposerStateMachine(FileSelector &fs, const char *m, unsigned v,
unsigned n, const char (*a)[IP_LENGTH]):
PaxosStateMachine(m){
	this->version = v;
	this->fs = &fs;
	for(unsigned i = 0; i != MAX_ACCEPTORS; i++)
		accsfd[i] = -1;
	this->nacceptor = n;
	this->proposal = new NoProposal();
	this->initialState();
	this->connectAcceptors(a);
	this->timestamp = getSecond();
}

ProposerStateMachine::~ProposerStateMachine(){
	delete this->proposal;
}

void ProposerStateMachine::disconnectAcceptors(){
	for(unsigned i = 0; i != MAX_ACCEPTORS; i++){
		if(accsfd[i] >= 0){
			this->fs->unregisterFD(accsfd[i]);
			close(accsfd[i]);
		}
		accsfd[i] = -1;
	}
}

unsigned ProposerStateMachine::connectAcceptors(const char (*acceptors)[IP_LENGTH]){
	unsigned successcount = 0;
	for(unsigned i = 0; i != this->nacceptor; i++){
		accsfd[i] = tcpconnect(acceptors[i], PAXOS_PORT);
		if(accsfd[i] < 0)
			accsfd[i] = -1;
		else{
			successcount++;
			this->fs->registerFD(accsfd[i]);
		}
	}
	return successcount;
}

void ProposerStateMachine::newProposal(Proposal* newproposal){
	delete this->proposal;
	this->proposal = newproposal;
	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(this->accsfd[i],
		PHASE1PROPOSAL, this->version, this->getName(), this->proposal);
	}
	this->state = PHASE1;
	this->timestamp=getSecond();
}

enum PaxosResult ProposerStateMachine::checkTimeout(double timeout){
	if(getSecond() - this->timestamp <= timeout)
		return IGNORED;
	switch(this->state){
	case INITIAL:
	case PHASE3:
		return IGNORED;
	case PHASE1:
	case PHASE2:
		this->initialState();
		// disconnectAcceptors();
		// this->connectAcceptors
		return HANDLED;
	}
	return IGNORED;
}

enum PaxosResult ProposerStateMachine::handlePhase1Message(enum PaxosMessageType t){
	if(this->state != PHASE1)
		return HANDLED;

	switch(t){
	case PHASE1ACK:
		this->phase1ack++;
		break;
	case PHASE1NACK:
		this->phase1nack++;
		break;
	default:
		return HANDLED;
	}
	if(this->phase1ack * 2 <= this->nacceptor && this->nacceptor != 0)
		return HANDLED;

	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(accsfd[i],
		PHASE2REQUEST, this->version, this->getName(), this->proposal);
	}
	this->state = PHASE2;
	this->timestamp = getSecond();
	return HANDLED;
}

enum PaxosResult ProposerStateMachine::handlePhase2Message(enum PaxosMessageType t){
	if(this->state != PHASE2)
		return HANDLED;

	switch(t){
	case PHASE2ACK:
		this->phase2ack++;
		break;
	case PHASE2NACK:
		this->phase2nack++;
		break;
	default:
		return HANDLED;
	}
	if(this->phase2ack * 2 <= this->nacceptor && this->nacceptor != 0)
		return HANDLED;

	return COMMITTING;
}

Proposal *ProposerStateMachine::getCommittingProposal(){
	if(this->state != PHASE2 ||
	(this->phase2ack * 2 <= this->nacceptor && this->nacceptor != 0))
		return NULL;
	return this->proposal;
}

void ProposerStateMachine::commit(){
	this->disconnectAcceptors();
	this->state = PHASE3;
	this->timestamp = getSecond();

	if(this->proposal->getType() != ACCEPTORCHANGEPROPOSAL)
		return;
	AcceptorChangeProposal *ap = (AcceptorChangeProposal*)(this->proposal);
	this->initialState();
	this->version = ap->getVersion();
	this->nacceptor = ap->getNumberOfAcceptors();
	this->connectAcceptors(ap->getAcceptors());
}

void ProposerStateMachine::sendCommitment(){
	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(accsfd[i],
		PHASE3COMMIT, this->version, this->getName(), this->proposal);
	}

	this->commit();
}

enum PaxosResult ProposerStateMachine::handlePhase3Message(Proposal *p){
	delete this->proposal;
	this->proposal = p;

	this->commit();
	return HANDLED;
}

enum PaxosResult ProposerStateMachine::handleMessage(int replysfd, PaxosMessage &m){
	struct PaxosHeader ph = m.header;

	if(isRecepient(ph.name) == 0)
		return IGNORED;

	if(this->state == PHASE3){
		if(ph.type != PHASE3COMMIT)
			sendHeaderProposal(replysfd, PHASE3COMMIT,
			this->version, this->getName(), this->proposal);
		return HANDLED;
	}

	if(this->version > ph.version) // ack or nack for previous version
		return HANDLED;

	switch(ph.type){
	case PHASE1ACK:
	case PHASE1NACK:
		return this->handlePhase1Message(ph.type);
	case PHASE2ACK:
	case PHASE2NACK:
		return this->handlePhase2Message(ph.type);
	case PHASE3COMMIT:
		return this->handlePhase3Message(m.getProposalPtr());
	default:
		return IGNORED;
	}
}


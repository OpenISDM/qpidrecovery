/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include<cstring>
#include"socketlib.h"
#include"paxos.h"
#include"timestamp.h"
#include"common.h"
#include<unistd.h>

int receivePaxosHeader(int sfd, struct PaxosHeader &ph);

int sendPaxosHeader(int sfd, enum PaxosMessageType t, unsigned v, const char *m);
int sendHeaderProposal(int sfd, enum PaxosMessageType t, unsigned v, const char *m, Proposal *p);

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
	rp = p->sendProposal(sfd);
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
	return read(sfd, &ph, sph) == sph? sph: -1;
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
	this->proposal = NULL;
	switch(this->header.type){
	case PHASE1PROPOSAL:
	case PHASE2REQUEST:
	case PHASE3COMMIT:
		this->proposal = Proposal::receiveProposal(sfd);
		if(this->proposal == NULL)
			return -1;
		break;
	default:
		this->proposal = NULL;
	}
	return rh;
}

Proposal *PaxosMessage::getProposalPtr(){
	Proposal *p;
	p = this->proposal;
	this->proposal = NULL;
	return p;
}

// state machine
PaxosStateMachine::PaxosStateMachine(const char *m, Proposal *p, unsigned v, enum PaxosState s, double t){
	strcpy(this->name, m);
	this->lastcommit = p;
	this->version = v;
	this->state = s;
	this->timestamp = t;
}

PaxosStateMachine::~PaxosStateMachine(){
	delete this->lastcommit;
}

int PaxosStateMachine::isRecepient(const char *m){
	return strcmp(m, this->name) == 0;
}

unsigned PaxosStateMachine::getVotingVersion(){
	return this->version;
}

const char *PaxosStateMachine::getName(){
	return this->name;
}

// class AcceptorStateMachine
/*
void AcceptorStateMachine::initialState(unsigned v){
	this->version = v;
	delete this->promise;
	this->promise = new NoProposal();
	this->state = PHASE1;
	this->timestamp = getSecond();
}
*/
AcceptorStateMachine::AcceptorStateMachine(const char *m, unsigned v, Proposal *last):
PaxosStateMachine(m, last, v, PHASE1, getSecond()){
	this->promise = new NoProposal();
}

AcceptorStateMachine::~AcceptorStateMachine(){
	delete this->promise;
}

enum PaxosResult AcceptorStateMachine::checkTimeout(double timeout){
	if(getSecond() - this->timestamp <= timeout)
		return IGNORED;

	switch(this->state){
	case INITIAL:
		// assert(0);
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
	if(this->state == PHASE3)
		sendHeaderProposal(replysfd, PHASE3COMMIT, v, this->getName(), this->lastcommit);
	if(this->state != PHASE1){
		delete p;
		return HANDLED;
	}

	enum PaxosMessageType reply;
	if(this->promise->compare(*p) > 0){
STDCOUT("acceptor: phase1 nack\n");
		delete p;
		reply = PHASE1NACK;
	}
	else{
STDCOUT("acceptor: phase1 ack\n");
		delete this->promise;
		this->promise = p;
		reply = PHASE1ACK;
		this->timestamp = getSecond();
	}
	sendPaxosHeader(replysfd, reply, v, this->getName());
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handlePhase2Message(int replysfd, Proposal *p, unsigned v){
	if(this->state == PHASE3)
		sendHeaderProposal(replysfd, PHASE3COMMIT, v, this->getName(), this->lastcommit);
	if(this->state != PHASE1){
		delete p;
		return HANDLED;
	}

	enum PaxosMessageType reply;
	if(this->promise->compare(*p) != 0){
STDCOUT("acceptor: phase2 nack\n");
		reply = PHASE2NACK;
	}
	else{
STDCOUT("acceptor: phase2 ack\n");
		reply = PHASE2ACK;
		this->timestamp = getSecond();
		this->state = PHASE2;
	}
	delete p;
	sendPaxosHeader(replysfd, reply, v, this->getName());
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handlePhase3Message(Proposal *p){
	if(this->state == PHASE3){
		delete p;
		return HANDLED;
	}
STDCOUT("acceptor: commit\n");
	delete this->lastcommit;
	this->lastcommit = p;
	this->state = PHASE3;
	this->timestamp = getSecond();
	return HANDLED;
}

enum PaxosResult AcceptorStateMachine::handleMessage(int replysfd, PaxosMessage &m){
	struct PaxosHeader &ph = m.header;
	if(this->isRecepient(ph.name) == 0)
		return IGNORED;
// STDCOUT(this->getVotingVersion() << " "<< ph.version << "\n");
// STDCOUTFLUSH();
	if(this->getVotingVersion() < ph.version){ // missed changing acceptor
		// this->initialState(ph.version);
		// reply the proposer
		return HANDLED;
	}

	if(this->getVotingVersion() > ph.version){ // outdated proposer
		if(this->lastcommit->getVersion() >= ph.version)
			sendHeaderProposal(replysfd, PHASE3COMMIT,
			ph.version, this->getName(), this->lastcommit);
		return HANDLED;
	}
	// else if(this->version == ph.version)
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
	this->phase2ack = this->phase2nack = 0;
	this->state = INITIAL;
	this->timestamp = getSecond();
}

ProposerStateMachine::ProposerStateMachine(FileSelector &fs, const char *m, unsigned v,
AcceptorChangeProposal *last):
PaxosStateMachine(m, last, v, INITIAL, getSecond()){
	this->fs = &fs;
	this->proposal = new NoProposal();
	this->phase1ack = this->phase1nack = 0;
	this->phase2ack = this->phase2nack = 0;
	for(unsigned i = 0; i != MAX_ACCEPTORS; i++)
		accsfd[i] = -1;
	this->nacceptor = last->getNumberOfAcceptors();
	this->connectAcceptors(last->getAcceptors());
}

ProposerStateMachine::~ProposerStateMachine(){
	disconnectAcceptors();
	delete this->proposal;
}

void ProposerStateMachine::disconnectAcceptors(){
	for(unsigned i = 0; i != MAX_ACCEPTORS; i++){
		if(accsfd[i] < 0)
			continue;
		this->fs->unregisterFD(accsfd[i]);
		close(accsfd[i]);
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

enum PaxosResult ProposerStateMachine::newProposal(Proposal* newproposal){
	delete this->proposal;
	this->proposal = newproposal;
STDCOUT("proposer: new proposal, " << this->nacceptor << " acceptors\n");
	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(this->accsfd[i],
		PHASE1PROPOSAL, this->getVotingVersion(), this->getName(), this->proposal);
	}

	this->timestamp=getSecond();

	if(this->nacceptor != 0){
		this->state = PHASE1;
		return HANDLED;
	}
	else{
STDCOUT("proposer: commit\n");
		this->state = PHASE2;
		return COMMITTING_SELF;
	}
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
		// this->connectAcceptors();
		return HANDLED;
	}
	return IGNORED;
}

enum PaxosResult ProposerStateMachine::handlePhase1Message(int replysfd, enum PaxosMessageType t){
	if(this->state == PHASE3)
		sendHeaderProposal(replysfd, PHASE3COMMIT,
		this->getVotingVersion(), this->getName(), this->lastcommit);
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
STDCOUT("proposer: phase1 ack=" << this->phase1ack << ", nack=" << this->phase1nack << "\n");
	if(this->phase1ack * 2 <= this->nacceptor && this->nacceptor != 0)
		return HANDLED;

	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(accsfd[i],
		PHASE2REQUEST, this->getVotingVersion(), this->getName(), this->proposal);
	}
STDCOUT("proposer: phase2 ack=0, nack=0\n");
	this->state = PHASE2;
	this->timestamp = getSecond();
	return HANDLED;
}


enum PaxosResult ProposerStateMachine::handlePhase2Message(int replysfd, enum PaxosMessageType t){
	if(this->state == PHASE3){
		sendHeaderProposal(replysfd, PHASE3COMMIT,
		this->getVotingVersion(), this->getName(), this->lastcommit);
		return HANDLED;
	}
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
STDCOUT("proposer: phase2 ack=" << this->phase2ack << ", nack=" << this->phase2nack << "\n");
	if(this->isReadyToCommit() == false)
		return HANDLED;

STDCOUT("proposer: commit\n");
	return COMMITTING_SELF;
}

bool ProposerStateMachine::isReadyToCommit(){
	return (this->state == PHASE2 &&
	(this->phase2ack * 2 > this->nacceptor || this->nacceptor == 0));
}

Proposal *ProposerStateMachine::getCommittingProposal(){
	if(this->isReadyToCommit())
		return this->proposal;
	if(this->state == PHASE3)
		return this->lastcommit;
STDCOUT("proposer: getCommittingProposal error\n");
STDCOUTFLUSH();
	return NULL;
}

void ProposerStateMachine::commit(Proposal *p){
	this->disconnectAcceptors();
	this->state = PHASE3;
	this->timestamp = getSecond();
	delete this->lastcommit;
	this->lastcommit = p;
}

void ProposerStateMachine::sendCommitment(){
	if(this->isReadyToCommit() == false)
		return;

	Proposal *p = this->proposal;
	this->proposal = new NoProposal();

	for(unsigned i = 0; i != this->nacceptor; i++){
		if(accsfd[i] < 0)
			continue;
		sendHeaderProposal(accsfd[i], PHASE3COMMIT, this->getVotingVersion(), this->getName(), p);
	}

	this->commit(p);
}

enum PaxosResult ProposerStateMachine::handlePhase3Message(Proposal *p){
	if(this->state == PHASE3){
		delete p;
		return HANDLED;
	}
STDCOUT("proposer: other proposer committed\n");
	this->commit(p);
	return COMMITTING_OTHER;
}

enum PaxosResult ProposerStateMachine::handleMessage(int replysfd, PaxosMessage &m){
	struct PaxosHeader &ph = m.header;

	if(this->isRecepient(ph.name) == 0)
		return IGNORED;

	if(this->getVotingVersion() < ph.version){ // invalid case
		return HANDLED;
	}
	if(this->getVotingVersion() > ph.version){ // reply of previous proposal
		sendHeaderProposal(replysfd, PHASE3COMMIT,
		ph.version, this->getName(), this->lastcommit);
		return HANDLED;
	}

	switch(ph.type){
	case PHASE1ACK:
	case PHASE1NACK:
		return this->handlePhase1Message(replysfd, ph.type);
	case PHASE2ACK:
	case PHASE2NACK:
		return this->handlePhase2Message(replysfd, ph.type);
	case PHASE3COMMIT:
		return this->handlePhase3Message(m.getProposalPtr());
	default:
		return IGNORED;
	}
}


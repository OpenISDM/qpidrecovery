#include"common.h"
#include"paxos.h"
#include<unistd.h>
#include<cstring>

Proposal::Proposal(enum ProposalType t, unsigned v){
	this->type = t;
	this->newversion = v;
}

Proposal::~Proposal(){	
}

int Proposal::sendType(int sfd){
	const int spt = sizeof(enum ProposalType);
	int r = -1;
	r = write(sfd, &(this->type), spt);
	return r == spt? r: -1;
}

enum ProposalType Proposal::getType(){
	return this->type;
}

unsigned Proposal::getVersion(){
	return this->newversion;
}

// getValue
int NoProposal::getValue(){
	return -2;
}

int RecoveryProposal::getValue(){
	return this->score;
}

int AcceptorChangeProposal::getValue(){
	return -1;
}

// compare
int NoProposal::compare(Proposal &p){
	if(this->getType() != p.getType())
		return this->getValue() > p.getValue()? 1: -1;
	return 0;
}

#define UNEQUALRETURN(A,B) if((A)!=(B)){return (A)>(B)? 1: -1;}
int RecoveryProposal::compare(Proposal &p){
	if(this->getType() != p.getType())
		return this->getValue() > p.getValue()? 1: -1;

	int c;
	RecoveryProposal *r = (RecoveryProposal*)&p;
	UNEQUALRETURN(this->score, r->score)
	if((c = strcmp(this->backupip, r->backupip)) != 0)
		return c;
	if((c = strcmp(this->failip, r->failip)) != 0)
		return c;
	UNEQUALRETURN(this->backupport, r->backupport)
	UNEQUALRETURN(this->failport, r->failport)
	return 0;
}

int AcceptorChangeProposal::compare(Proposal &p){
	if(this->getType() != p.getType())
		return this->getValue() > p.getValue()? 1: -1;

	AcceptorChangeProposal *a = (AcceptorChangeProposal*)&p;
	UNEQUALRETURN(this->nacceptor, a->nacceptor)

	for(unsigned i = 0; i < this->nacceptor; i++){ // does order matter?
		int c = strcmp(this->acceptor[i], a->acceptor[i]);
		if(c != 0) 
			return c;
	}
	return 0;
}

template<typename T>
void rwType(int rw, int sfd, T &buff, int &returnsum){
	int r;
	if(returnsum < 0)
		return;

	r = -1;
	if(rw == 'w')
		r = write(sfd, &buff, sizeof(T));
	if(rw == 'r')
		r = read(sfd, &buff, sizeof(T));
	if(r != sizeof(T))
		returnsum = -1;
	else
		returnsum+=r;
}

int Proposal::sendProposal(int sfd){
	int t = 0;
	rwType('w', sfd, this->type, t);
	rwType('w', sfd, this->newversion, t);
	int rc = this->sendReceiveContent('w', sfd);
	return (t < 0 || rc < 0)? -1: t + rc;
}

Proposal *Proposal::receiveProposal(int sfd){
	enum ProposalType proposaltype;
	unsigned newversion;
	Proposal *p;
	{
		int t = 0;
		rwType('r', sfd, proposaltype, t);
		rwType('r', sfd, newversion, t);
		if(t < 0)
			return NULL;
	}
	switch(proposaltype){
	case NOPROPOSAL:
		return NULL;
	case RECOVERYPROPOSAL:
		p = new RecoveryProposal(newversion);
		break;
	case ACCEPTORCHANGEPROPOSAL:
		p = new AcceptorChangeProposal(newversion);
		break;
	default:
		return NULL;
	}
	if(p->sendReceiveContent('r', sfd) < 0){
		delete p;
		return NULL;
	}
	return p;
}

// class NoProposal
NoProposal::NoProposal():
Proposal(NOPROPOSAL, 0){
}

int NoProposal::sendReceiveContent(int rw, int sfd){
	if(sfd >= 0 && (rw == 'r' || rw == 'w'))
		return 0;
	return -1;
}

// class RecoveryProposal
RecoveryProposal::RecoveryProposal(unsigned ver, const char *fip, unsigned fport,
const char *bip, unsigned bport, unsigned bscore):
Proposal(RECOVERYPROPOSAL, ver){
	strcpy(this->failip, fip);
	this->failport = fport;
	strcpy(this->backupip, bip);
	this->backupport = bport;
	this->score = bscore;
}

int RecoveryProposal::sendReceiveContent(int rw, int sfd){
	int t = 0;
	rwType(rw, sfd, this->failip, t);
	rwType(rw, sfd, this->failport, t);
	rwType(rw, sfd, this->backupip, t);
	rwType(rw, sfd, this->backupport, t);
	rwType(rw, sfd, this->score, t);
	return t;
}

const char *RecoveryProposal::getBackupIP(){
	return this->backupip;
}

unsigned RecoveryProposal::getBackupPort(){
	return this->backupport;
}

const char *RecoveryProposal::getFailIP(){
	return this->failip;
}

unsigned RecoveryProposal::getFailPort(){
	return this->failport;
}

//class AcceptorChangeProposal
AcceptorChangeProposal::AcceptorChangeProposal(unsigned ver,
unsigned nnewlist, const char (*newlist)[IP_LENGTH]):
Proposal(ACCEPTORCHANGEPROPOSAL, ver){
	this->nacceptor = nnewlist;
	for(unsigned i = 0; i < nnewlist; i++)
		strcpy(this->acceptor[i], newlist[i]);
}

int AcceptorChangeProposal::sendReceiveContent(int rw, int sfd){
	int t = 0;
	rwType(rw, sfd, this->nacceptor, t);
	rwType(rw, sfd, this->acceptor, t);
	return t;
}

const char (*AcceptorChangeProposal::getAcceptors())[IP_LENGTH]{
	return this->acceptor;
}

unsigned AcceptorChangeProposal::getNumberOfAcceptors(){
	return this->nacceptor;
}



#include"common.h"
#include"paxos.h"
#include<unistd.h>
#include<cstring>

Proposal::Proposal(enum ProposalType t){
	this->type = t;
}

int Proposal::sendTypeContent(int sfd){
	int rt, rc;
	rt = this->sendType(sfd);
	if(rt < 0)
		return -1;
	rc = this->sendReceiveContent('w', sfd);
	if(rc < 0)
		return -1;
	return rt + rc;
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

// isEqual
bool NoProposal::isEqual(Proposal &p){
	if(this->getType() != p.getType())
		return false;
	return true;
}

bool RecoveryProposal::isEqual(Proposal &p){
	if(this->getType() != p.getType())
		return false;
	RecoveryProposal *r = (RecoveryProposal*)&p;

	if(this->score != r->score ||
	strcmp(this->backupip, r->backupip) != 0 ||
	strcmp(this->failip, r->failip) != 0 ||
	this->backupport != r->backupport ||
	this->failport != r->failport)
		return false;
	return true;
}

bool AcceptorChangeProposal::isEqual(Proposal &p){
	if(this->getType() == p.getType())
		return false;

	AcceptorChangeProposal *a = (AcceptorChangeProposal*)&p;
	if(this->nacceptor != a->nacceptor)
		return false;
	for(unsigned i = 0; i < this->nacceptor; i++){ // does order matter?
		if(strcmp(this->acceptor[i], a->acceptor[i]) != 0) 
			return false;
	}
	return true;
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

// class NoProposal
NoProposal::NoProposal():
Proposal(NOPROPOSAL){
}

int NoProposal::sendReceiveContent(int rw, int sfd){
	if(sfd >= 0 && (rw == 'r' || rw == 'w'))
		return 0;
	return -1;
}

// class RecoveryProposal
RecoveryProposal::RecoveryProposal(const char *fip, unsigned fport,
const char *bip, unsigned bport, unsigned bscore):
Proposal(RECOVERYPROPOSAL){
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
AcceptorChangeProposal::AcceptorChangeProposal(unsigned newversion,
unsigned nnewlist, const char (*newlist)[IP_LENGTH]):
Proposal(ACCEPTORCHANGEPROPOSAL){
	this->nacceptor = nnewlist;
	for(unsigned i = 0; i < nnewlist; i++)
		strcpy(this->acceptor[i], newlist[i]);
	this->version = newversion;
}

int AcceptorChangeProposal::sendReceiveContent(int rw, int sfd){
	int t = 0;
	rwType(rw, sfd, this->nacceptor, t);
	rwType(rw, sfd, this->acceptor, t);
	rwType(rw, sfd, this->version, t);
	return t;
}

const char (*AcceptorChangeProposal::getAcceptors())[IP_LENGTH]{
	return this->acceptor;
}

unsigned AcceptorChangeProposal::getNumberOfAcceptors(){
	return this->nacceptor;
}

int AcceptorChangeProposal::getVersion(){
	return this->version;
}


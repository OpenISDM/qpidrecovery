#include<cstring>
#include"paxos.h"
#include<unistd.h>

// class Proposal
int NoProposal::getValue(){
	return -2;
}

int RecoveryProposal::getValue(){
	return this->score;
}

int AcceptorChangeProposal::getValue(){
	return -1;
}

bool operator>(Proposal &p1, Proposal &p2){
	return p1.getValue() > p2.getValue();
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
NoProposal::NoProposal(){
		this->type = NOPROPOSAL;
}

int NoProposal::sendReceiveContent(int rw, int sfd){
	if(sfd >= 0 && (rw == 'r' || rw == 'w'))
		return 0;
	return -1;
}

// class RecoveryProposal
RecoveryProposal::RecoveryProposal(const char *backupip, unsigned backupport, unsigned backupscore){
	strcpy(this->ip, backupip);
	this->port = backupport;
	this->score = backupscore;
}

int RecoveryProposal::sendReceiveContent(int rw, int sfd){
	int t = 0;
	rwType(rw, sfd, this->ip, t);
	rwType(rw, sfd, this->port, t);
	rwType(rw, sfd, this->score, t);
	return t;
}

//class AcceptorChangeProposal
AcceptorChangeProposal::AcceptorChangeProposal(unsigned nnewlist, const char (*newlist)[IP_LENGTH]){
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

// state machine

int PaxosStateMachine::isRecepient(struct PaxosHeader &ph){
	return strcmp(ph.name, this->name) == 0;
}

int AcceptorStateMachine::handleMessage(struct PaxosHeader &ph, int sfd){
	if(isRecepient(ph) == 0)
		return -1;

	switch(ph.type){
	case PHASE1PROPOSAL:
		break;
	case PHASE2REQUEST:
		break;
	case PHASE3COMMIT:
		break;
	default:
		return -1;
	}
	return 0;
}

int ProposerStateMachine::handleMessage(struct PaxosHeader &ph, int sfd){
	if(isRecepient(ph) == 0)
		return -1;

	switch(ph.type){
	case PHASE1ACK:
		break;
	case PHASE1NACK:
		break;
	case PHASE2ACK:
		break;
	case PHASE2NACK:
		break;
	case PHASE3COMMIT:
		break;
	case NEWVERSION:
		break;
	default:
		return -1;
	}
	return 0;
}

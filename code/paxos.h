#include"common.h"
enum PaxosMessageType{
	PHASE1PROPOSAL = 555, // body = Proposal
	PHASE1ACK, // empty
	PHASE1NACK, // empty
	PHASE2REQUEST, // body = Proposal
	PHASE2ACK, // empty
	PHASE2NACK, // empty
	PHASE3COMMIT, // empty
	NEWVERSION
};

enum PaxosState{
	INITIAL = 777,
	PHASE1,
	PHASE2,
	PHASE3
};

struct PaxosHeader{
	enum PaxosMessageType type;
	unsigned version;
	char name[SERVICE_NAME_LENGTH];
};

enum ProposalType{
	NOPROPOSAL = 999,
	RECOVERYPROPOSAL,
	ACCEPTORCHANGEPROPOSAL
} type;

class Proposal{
protected:
	enum ProposalType type;

public:
	virtual int SendReceiveContent(int rw, int sfd) = 0;
	// rw = 'r' or 'w'
	virtual int getValue() = 0;
};

bool operator>(Proposal &p1, Proposal &p2);
Proposal *receiveProposal(int sfd);
void sendProposal(int sfd, Proposal *p);

class NoProposal: public Proposal{
public:
	NoProposal();
	int sendReceiveContent(int rw, int sfd);
	int getValue();
};

class RecoveryProposal: public Proposal{
private:
	char ip[IP_LENGTH];
	unsigned port;
	unsigned score;

public:
	RecoveryProposal(const char *backupip = "", unsigned backupport = 0, unsigned backupscore = 0);
	int sendReceiveContent(int rw, int sfd);
	int getValue();
};

class AcceptorChangeProposal: public Proposal{
private:
	unsigned nacceptor;
	char acceptor[MAX_ACCEPTORS][IP_LENGTH];

public:
	AcceptorChangeProposal(unsigned nnewlist = 0, const char (*newlist)[IP_LENGTH] = NULL);
	int sendReceiveContent(int rw, int sfd);
	int getValue();
};

class PaxosStateMachine{
protected:
	unsigned version;
	unsigned nacceptor;
	char acceptors[MAX_ACCEPTORS][IP_LENGTH];

	enum PaxosState state; // type of last send/received message
	char name[SERVICE_NAME_LENGTH]; // monitored service name

	int isRecepient(struct PaxosHeader &ph);
public:
	virtual int handleMessage(struct PaxosHeader &ph, int sfd) = 0;
};

class AcceptorStateMachine:public PaxosStateMachine{
private:
	struct Proposal*promise;
public:
	int handleMessage(struct PaxosHeader &ph, int sfd);
};

class ProposerStateMachine:public PaxosStateMachine{
private:
	struct Proposal*proposal;
public:
	int newProposal();
	int handleMessage(struct PaxosHeader &ph, int sfd);
};

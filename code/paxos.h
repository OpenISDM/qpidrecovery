#include"common.h"

enum PaxosMessageType{
	PHASE1PROPOSAL = 555, // body = Proposal
	PHASE1ACK, // empty
	PHASE1NACK, // empty
	PHASE2REQUEST, // Proposal
	PHASE2ACK, // empty
	PHASE2NACK, // empty
	PHASE3COMMIT, // Proposal
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
};

class Proposal{
private:
	enum ProposalType type;

public:
	Proposal(enum ProposalType t);
	int sendTypeContent(int sfd);
	int sendType(int sfd);
	enum ProposalType getType();

	virtual int sendReceiveContent(int rw, int sfd) = 0;
	// rw = 'r' or 'w'
	virtual bool isEqual(Proposal &p) = 0;
	virtual int getValue() = 0;
};

class NoProposal: public Proposal{
public:
	NoProposal();
	int sendReceiveContent(int rw, int sfd);
	int getValue();
	bool isEqual(Proposal &p);
};

class RecoveryProposal: public Proposal{
private:
	char failip[IP_LENGTH];
	unsigned failport;
	char backupip[IP_LENGTH];
	unsigned backupport;
	unsigned score;

public:
	RecoveryProposal(const char *fip = "", unsigned fport = 0,
	const char *bip = "", unsigned bport = 0, unsigned bscore = 0);
	int sendReceiveContent(int rw, int sfd);
	int getValue();
	bool isEqual(Proposal &p);

	const char*getBackupIP();
	unsigned getBackupPort();
	const char*getFailIP();
	unsigned getFailPort();
};

class AcceptorChangeProposal: public Proposal{
private:
	unsigned nacceptor;
	char acceptor[MAX_ACCEPTORS][IP_LENGTH];
	unsigned version;

public:
	AcceptorChangeProposal(unsigned newversion = 0,
	unsigned nnewlist = 0, const char (*newlist)[IP_LENGTH] = NULL);
	int sendReceiveContent(int rw, int sfd);
	int getValue();
	bool isEqual(Proposal &p);

	const char (*getAcceptors())[IP_LENGTH];
	unsigned getNumberOfAcceptors();
	int getVersion();
};

class PaxosMessage{
public:
	struct PaxosHeader header;
	Proposal* proposal;

	PaxosMessage();
	int receive(int sfd);
	Proposal *getProposalPtr();
	~PaxosMessage();
};

enum PaxosResult{
	HANDLED = 3333,
	IGNORED,
	COMMITTING
};

class PaxosStateMachine{
private:
	// requestor name. cannot change except in constructor
	char name[SERVICE_NAME_LENGTH];
protected:
	unsigned version;
	enum PaxosState state; // managed by subclass
	unsigned timestamp; // managed by subclass

	PaxosStateMachine(const char *m);

	const char *getName();
public:
	int isRecepient(const char *m);
	// return 1 if state transition, 0 if not
	virtual enum PaxosResult checkTimeout(double timeout) = 0;
	// return -1 if wrong message, 0 if handled
	virtual enum PaxosResult handleMessage(int replysfd, PaxosMessage &m) = 0;
};

class AcceptorStateMachine:public PaxosStateMachine{
private:
	Proposal*promise;

	unsigned nacceptor;
	char acceptors[MAX_ACCEPTORS][IP_LENGTH];

	enum PaxosResult handlePhase1Message(int replysfd, Proposal *p, unsigned v);
	enum PaxosResult handlePhase2Message(int replysfd, Proposal *p, unsigned v);
	enum PaxosResult handlePhase3Message(Proposal *p);
	void initialize(unsigned v, unsigned n, const char (*a)[IP_LENGTH]);
public:
	AcceptorStateMachine(const char *m, unsigned v,
	unsigned n, const char (*a)[IP_LENGTH]);
	~AcceptorStateMachine();

	enum PaxosResult checkTimeout(double timeout);
	enum PaxosResult handleMessage(int replysfd, PaxosMessage &m);
};

#include"fileselector.h"

class ProposerStateMachine:public PaxosStateMachine{
private:
	Proposal *proposal;
	unsigned phase1ack, phase1nack;
	unsigned phase2ack, phase2nack;

	unsigned nacceptor;
	int accsfd[MAX_ACCEPTORS]; // fd = -1 if not in use, >= 0 tcp socket
	FileSelector* fs;

	enum PaxosResult handlePhase1Message(enum PaxosMessageType t);
	enum PaxosResult handlePhase2Message(enum PaxosMessageType t);
	enum PaxosResult handlePhase3Message(Proposal *p);
	void initialState();
	void disconnectAcceptors();
	unsigned connectAcceptors(const char (*acceptors)[IP_LENGTH]);
	void commit();
public:
	ProposerStateMachine(FileSelector &fs, const char *m, unsigned v,
	unsigned n, const char (*a)[IP_LENGTH]);
	~ProposerStateMachine();

	void newProposal(Proposal *newproposal);
	enum PaxosResult checkTimeout(double timeout);
	const int *getAcceptorFD();

	enum PaxosResult handleMessage(int replysfd, PaxosMessage &m);

	Proposal *getCommittingProposal();
	void sendCommitment();
};


#include"common.h"
#include<vector>
#include"fileselector.h"

struct AddressChange{
	char name[SERVICE_NAME_LENGTH];
	unsigned count;
	unsigned livingcount;
	char proposers[MAX_PROPOSERS][IP_LENGTH];
	int sfd[MAX_PROPOSERS];

	char srcip[IP_LENGTH];
	unsigned srcport;
	char oldip[IP_LENGTH];
	unsigned oldport;
};

class LinkDstListener{
private:
	FileSelector*fs;
	std::vector<struct AddressChange> listening;

public:
	LinkDstListener(FileSelector &fileselector);

	int listenAddressChange(const ReplyProposer &rp,
	const char *sip, unsigned sport, const char *oip, unsigned oport);

	// return -1 if error, 0 if no found, 1 if changed to newip:newport, 2 if no change
	int readAddressChange(int ready,
	char *sip, unsigned &sport,
	char *oip, unsigned &oport,
	char *newip, unsigned &newport);
};


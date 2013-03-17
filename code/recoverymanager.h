#include<vector>
#include"listener.h"
#include"brokeraddress.h"
#include"brokerobject.h"

typedef vector<ObjectInfo*> ObjectInfoPtrVec;

struct BrokerAddressPair{
	BrokerAddress address;
	Broker* pointer;
};
typedef vector<struct BrokerAddressPair> AddressPairVec;
typedef vector<struct BrokerAddress> BrokerAddressVec;

class RecoveryManager{
private:
	Listener *listener;
	SessionManager *sm;
	vector<ObjectInfo*> exchanges, queues, bindings, brokers, links, bridges;
	AddressPairVec bavec;
	//BrokerAddressVec altbavec;

	int getBrokerIndex(BrokerAddress& ba);

	// return 1 if ignored, 0 if added
	int addObjectInfo(ObjectInfo* objinfo, enum ObjectType objtype);

public:
	RecoveryManager();
	~RecoveryManager();

	int addBroker(const char *ip, unsigned port);
};


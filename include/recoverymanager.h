#include<vector>
#include"pthread.h"
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
	pthread_mutex_t bavecmutex;
	int eventpipe[2];

	int getBrokerIndexByAddress(BrokerAddress& ba);

	// return 1 if ignored, 0 if added
	int addObjectInfo(ObjectInfo* objinfo, enum ObjectType objtype);

public:
	RecoveryManager();
	~RecoveryManager();

	int getEventFD();
	ListenerEvent *handleEvent();
	// recovery
	int addBroker(const char *ip, unsigned port);
	int deleteBroker(const char *ip, unsigned port);

	int copyObjects(const char *failip, unsigned failport,
	const char *backupip, unsigned backupport);

	int reroute(const char *srcip, unsigned srcport,
	const char *oldip, unsigned oldport, const char *newip, unsigned newport);

	friend void *addBrokerThread(void *voidba);
	// friend void *copyObjectsThread(void *voidcoarg);
};


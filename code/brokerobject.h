#ifndef BROKEROBJECT_H_INCLUDED
#define BROKEROBJECT_H_INCLUDED

#include "qpid/console/SessionManager.h"
#include <vector>
using namespace std;
using namespace qpid::console;

void urlToIPPort(string url, char *ip, unsigned &port);
string IPPortToUrl(const char*ip, unsigned port);

string linkObjectDest(Object& o);

//landmark_object.cpp
enum ObjectType{
    OBJ_BROKER,
    OBJ_LINK,
    OBJ_EXCHANGE,
    OBJ_QUEUE,
    OBJ_BINDING,
    OBJ_BRIDGE,
    NUMBER_OF_OBJ_TYPES,
    OBJ_UNKNOWN
};

string objectTypeToString(enum ObjectType ty);
enum ObjectType objectStringToType(string st);

class ObjectInfo{
private:
	enum ObjectType type; // OBJ_UNKNOWN by default
	ObjectId objectid;
	string brokerurl;
	Broker*brokerptr; // does the pointer change during operation?

	virtual void readArgs(Object& obj)=0;
public:
	ObjectInfo(Object& obj, Broker*broker, enum ObjectType ty=OBJ_UNKNOWN);
	void init(Object& obj, Broker*broker, enum ObjectType ty);
	//void initPhase2(SessionManager&sm);

	//Broker *getBrokerPtr();
	string getBrokerUrl();
	//Broker* getBrokerPtr();
	virtual int copyTo(Object* dest)=0;
	ObjectId getId();

	bool operator==(ObjectInfo& oi);
};

ObjectInfo *newObjectInfoByType(Object *object, Broker *broker, enum ObjectType t);

class BrokerInfo: public ObjectInfo{
private:
	void readArgs(Object& obj);
public:
	BrokerInfo(Object& obj, Broker*broker);
	int copyTo(Object* dest);
};

class LinkInfo: public ObjectInfo{
private:
	string host;
	unsigned port;

	void readArgs(Object& obj);
public:
	LinkInfo(Object& obj,Broker*broker);
	string getLinkDestHost();
	unsigned getLinkDestPort();
	string getLinkDestUrl();
	int copyToNewDest(Object* dest, string host, unsigned port);
	int copyTo(Object* dest);
};

class ExchangeInfo: public ObjectInfo{
private:
	string name, typestr;
	string extype;

	void readArgs(Object& obj);
public:
	ExchangeInfo(Object& obj, Broker*broker);
	string getName();
	int copyTo(Object* dest);
};

class QueueInfo: public ObjectInfo{
private:
	string name, typestr;
	unsigned long long deqcount;
	unsigned depth;

	void readArgs(Object& obj);
public:
	QueueInfo(Object& obj, Broker*broker);
	string getName();
	void setQueueStat(unsigned long long& count, unsigned& dep);// return old value
	int copyTo(Object* dest);
};

class BindingInfo: public ObjectInfo{
private:
	ObjectId exid, quid;//binding
	string bindingkey, exname, quname;// binding

	void readArgs(Object& obj);
public:
	BindingInfo(Object& obj, Broker*broker);

	//return 1 if set, 0 if name does not exist
	int setBindingNames(vector<ObjectInfo*>& exvec, vector<ObjectInfo*>& quvec);
	int copyTo(Object* dest);
};

class BridgeInfo: public ObjectInfo{
private:
	ObjectId linkid;//bridge
	string src, dest, routingkey;// bridge
	bool isqueue, isdynamic;// bridge

	void readArgs(Object& obj);
public:
	BridgeInfo(Object& obj, Broker*broker);
	ObjectId getLinkId();
	string getBridgeSrcUrl();
	string getBridgeDestUrl();
	int copyTo(Object* dest);
};

class SystemInfo{ // 1. should inherit ObjectInfo?  2. distinguish system in/outside subnet?
private:
    // ObjectId objectid;
    unsigned long long lasttotaldeq, totaldeq; // measure throughput
    long long totaldepth;
    double deqrate;
    struct PingdData{ // measure number of hops
        string destip;
        int ttl;
        time_t lastreturn;
        // time_t lastrequest;
    }* hoptable;
    int nhoptable;
    int ttlvalue;
    char ipaddress[32];
    const static int large_ttl = 1024;
public:
    SystemInfo(/*ObjectId oid, */const char* ipaddr);
    ~SystemInfo();
    // ObjectId getId();
    string getAddress();
    int getDefaultTTL();
    void addStatistics(unsigned long long countchange, long long depthchange);
    void updateDequeueRate(double interval);
    double getDequeueRate();
    void initPingdData(vector<SystemInfo>& systemvec, vector<SystemInfo>& outsystemvec);
    void initPingdData(SystemInfo& sysinfo);
    int getTTL(string destip, int& ttl); // return 0 = not expired, 1 = expired, -1 = wrong ip
    int setTTL(string destip, int ttl, time_t returntime);
};

class ObjectOperator{
private:
    Object* object;// see qpid/console/Object.h
    enum ObjectType type;
public:
    ObjectOperator(Object* o = NULL, enum ObjectType t = OBJ_UNKNOWN);
    void setObject(Object* o, enum ObjectType t);
    //broker operation
    int addExchange(string name, string extype,
    int durable = 0, string alternate = "");

    int addQueue(string name, int autodelete = 0,
    int durable = 0, string alternate = "");

    int addBinding(string key, string exname, string quname);

    int addLink(string host, unsigned port,
    string username = "guest", string password = "guest",
    bool durable = false, string auth = "", string transport = "tcp");

    //link operation
    int addBridge(string src, string dest, string key,
    bool isqueue, bool isdynamic,
    bool durable = false, string tag = "", string excludes = "",
    bool islocal = true/*assumption*/, int issync = 0);
};

//ifndef in the first line
#endif


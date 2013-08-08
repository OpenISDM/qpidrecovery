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
	enum ObjectType type;
	ObjectId objectid;
	string brokerurl;
//	Broker*brokerptr;

public:
	ObjectInfo(Object &obj, Broker *broker, enum ObjectType ty=OBJ_UNKNOWN);
	ObjectInfo(ObjectInfo*copysrc);
	virtual ~ObjectInfo();

	string getBrokerUrl();
	//Broker* getBrokerPtr();
	virtual int copyTo(Object* dest) = 0;

	ObjectId getId();

	virtual ObjectInfo *duplicate() = 0; // this function contains an operator new
	bool operator==(ObjectInfo& oi);
};

ObjectInfo *newObjectInfoByType(Object *object, Broker *broker, enum ObjectType t);

class BrokerInfo: public ObjectInfo{
public:
	BrokerInfo(Object& obj, Broker*broker);
	BrokerInfo(BrokerInfo *copysrc);
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
};

class LinkInfo: public ObjectInfo{
private:
	string host;
	unsigned port;

public:
	LinkInfo(Object& obj, Broker*broker);
	LinkInfo(LinkInfo *copysrc);
	string getLinkDestHost();
	unsigned getLinkDestPort();
	string getLinkDestUrl();
	int copyToNewDest(Object* dest, string host, unsigned port);
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
};

class ExchangeInfo: public ObjectInfo{
private:
	string name, typestr;
	string extype;

public:
	ExchangeInfo(Object& obj, Broker*broker);
	ExchangeInfo(ExchangeInfo *copysrc);
	string getName();
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
};

class QueueInfo: public ObjectInfo{
private:
	string name, typestr;
	unsigned long long deqcount;
	unsigned depth;

public:
	QueueInfo(Object& obj, Broker *broker);
	QueueInfo(QueueInfo *copysrc);
	string getName();
	void setQueueStat(unsigned long long& count, unsigned& dep);// return old value
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
};

class BindingInfo: public ObjectInfo{
private:
	ObjectId exid, quid;
	string bindingkey, exname, quname;

public:
	BindingInfo(Object& obj, Broker*broker);
	BindingInfo(BindingInfo *copysrc);

	//return 1 if set, 0 if name does not exist
	int setBindingNames(vector<ObjectInfo*>& exvec, vector<ObjectInfo*>& quvec);
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
};

class BridgeInfo: public ObjectInfo{
private:
	ObjectId linkid;
	string src, dest, routingkey;
	bool isqueue, isdynamic;

public:
	BridgeInfo(Object& obj, Broker*broker);
	BridgeInfo(BridgeInfo *copysrc);
	ObjectId getLinkId();
	int copyTo(Object* dest);
	ObjectInfo *duplicate();
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

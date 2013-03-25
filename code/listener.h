#ifndef LISTENER_H_DEFINED
#define LISTENER_H_DEFINED

#include"timestamp.h"
#include<qpid/console/ConsoleListener.h>
#include<qpid/console/SessionManager.h>
#include"brokerobject.h"

using namespace qpid::console;

enum ListenerEventType{
	BROKERDISCONNECTION = 142857,
	OBJECTPROPERTY
//	BYTEDEQUEUE,
//	BYTEENQUEUE,
};

class ListenerEvent{
private:
	enum ListenerEventType type;
protected:
	ListenerEvent(enum ListenerEventType t);
public:
	enum ListenerEventType getType();

	static int sendPtr(int fd, ListenerEvent *le);
	static ListenerEvent *receivePtr(int fd); // need to delete the ptr
};

class BrokerDisconnectionEvent: public ListenerEvent{
public:
	BrokerDisconnectionEvent(string url);
	char brokerip[32];
	unsigned brokerport;
};

class ObjectPropertyEvent: public ListenerEvent{
private:
	ObjectInfo *objinfo;
public:
	ObjectPropertyEvent(enum ObjectType t, ObjectInfo *i);
	~ObjectPropertyEvent();
	enum ObjectType objtype;
	ObjectInfo *getObjectInfo();
};

class Listener: public ConsoleListener {
private:
	int eventfd;
public:
	Listener(int pipewritefd);

	void brokerConnected(const Broker& broker);
	void brokerDisconnected(const Broker& broker);
	void newPackage(const std::string& name);
	void newClass(const ClassKey& classKey);
	void newAgent(const Agent& agent);
	void delAgent(const Agent& agent);
	void objectProps(Broker& broker, Object& object);
	void objectStats(Broker& broker, Object& object);
	void event(Event& event);
};

#endif


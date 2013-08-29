#include"listener.h"
#include"brokerobject.h"
#include<unistd.h>
#include<memory.h>
#include"timestamp.h"

//#define STDCOUT1(A) std::cout << A
#define STDCOUT1(A)
//#define STDCOUT2(A) std::cout << A
#define STDCOUT2(A)
#define STDCOUTFLUSH()
//#define STDCOUTFLUSH() std::cout.flush()

//class ListenerEvent
int ListenerEvent::sendEventPtr(int fd, ListenerEvent *le){
	return write(fd, &le, sizeof(ListenerEvent*));
}

ListenerEvent *ListenerEvent::receiveEventPtr(int fd){
	ListenerEvent *r;
	if(read(fd, &r, sizeof(ListenerEvent*)) != sizeof(ListenerEvent*))
		return NULL;
	return r;
}

ListenerEvent::ListenerEvent(enum ListenerEventType t){
	this->type = t;
}

enum ListenerEventType ListenerEvent::getType(){
	return this->type;
}

BrokerDisconnectionEvent::BrokerDisconnectionEvent(string url):
ListenerEvent(BROKERDISCONNECTION){
	urlToIPPort(url, this->brokerip, this->brokerport);
}

ObjectPropertyEvent::ObjectPropertyEvent(Object &obj):
ListenerEvent(OBJECTPROPERTY){
	this->objtype = objectStringToType(obj.getClassKey().getClassName());
	this->objinfo = newObjectInfoByType(&obj, this->objtype);
}

ObjectPropertyEvent::~ObjectPropertyEvent(){
	delete this->objinfo;
}

LinkDownEvent::LinkDownEvent(string srcurl, string dsturl, bool fromqmf):
ListenerEvent(LINKDOWN){
	urlToIPPort(srcurl, this->srcip, this->srcport);
	urlToIPPort(dsturl, this->dstip, this->dstport);
	this->isfromqmf = fromqmf;
}

//class Listener

Listener::Listener(int fd){
	this->eventfd = fd;
}

void Listener::brokerConnected(const Broker& broker){
	STDCOUT1("brokerConnected: " << broker << endl);
}

void Listener::brokerDisconnected(const Broker& broker){
	STDCOUT1("brokerDisconnected: " << broker << endl);

	STDCOUT2("broker disconnection event at " << getSecond() << endl);
	ListenerEvent *le = new BrokerDisconnectionEvent(broker.getUrl());
	int ret = ListenerEvent::sendEventPtr(this->eventfd, le);
	STDCOUT2("passing event: return " << ret << endl);
}

void Listener::newPackage(const std::string& name){
	STDCOUT1("newPackage: " << name << endl);
}

void Listener::newClass(const ClassKey& classKey){
	STDCOUT1("newClass: key=" << classKey << endl);
}

void Listener::newAgent(const Agent& agent){
	STDCOUT1("newAgent: " << agent << endl);
}
void Listener::delAgent(const Agent& agent){
	STDCOUT1("delAgent: " << agent << endl);
}

void Listener::objectProps(Broker& broker, Object& object){
	STDCOUT1("objectProps: broker=" << broker << " object=" << object << endl);
	STDCOUT2("object property event at " << getSecond() << endl);
	ListenerEvent *le = new ObjectPropertyEvent(object);
	if(((ObjectPropertyEvent*)le)->objinfo == NULL){
		delete le;
		return;
	}
	int ret = ListenerEvent::sendEventPtr(this->eventfd, le);
	STDCOUT2("send event: return " << ret << endl);
}

void Listener::objectStats(Broker& broker, Object& object){
	STDCOUT1("objectStats: broker=" << broker << " object=" << object << endl);
}

void Listener::sendLinkDownEvent(string srcurl, string dsturl, bool fromqmf){
	ListenerEvent *le = new LinkDownEvent(srcurl, dsturl, fromqmf);
	int ret = ListenerEvent::sendEventPtr(this->eventfd, le);
	STDCOUT2("send event: return " << ret << endl);
}

void Listener::event(Event& event){
	STDCOUT1("event: " << event << endl);

	const string linkdownstr = "brokerLinkDown";
	const struct SchemaClass* sc = event.getSchema();
	string classname = sc->key.getClassName();

	if(classname.compare(linkdownstr) != 0){
		return;
	}
	STDCOUT2("link down event at " << getSecond() << endl);
	//only report local route error
	this->sendLinkDownEvent(event.getBroker()->getUrl(), event.attrString("rhost"), true);
}


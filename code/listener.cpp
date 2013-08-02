#include"listener.h"
#include"brokerobject.h"
#include<unistd.h>
#include<memory.h>
#include"timestamp.h"

using namespace std;

//class ListenerEvent
int ListenerEvent::sendPtr(int fd, ListenerEvent *le){
	return write(fd, &le, sizeof(ListenerEvent*));
}

ListenerEvent *ListenerEvent::receivePtr(int fd){
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

ObjectPropertyEvent::ObjectPropertyEvent(enum ObjectType t, ObjectInfo *i):
ListenerEvent(OBJECTPROPERTY){
	this->objtype = t;
	this->objinfo = i;
}

ObjectInfo *ObjectPropertyEvent::getObjectInfo(){
	ObjectInfo *r = this->objinfo;
	this->objinfo = NULL;
	return r;
}

ObjectPropertyEvent::~ObjectPropertyEvent(){
	if(this->objinfo != NULL)
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
	cout << "brokerConnected: " << broker << endl;
}

void Listener::brokerDisconnected(const Broker& broker){
	cout << "brokerDisconnected: " << broker << endl;
	cout << "broker disconnection event at " << getSecond() << "\n";
	ListenerEvent *le = new BrokerDisconnectionEvent(broker.getUrl());
	cout << "passing event: return " << ListenerEvent::sendPtr(this->eventfd, le) << endl;
}

void Listener::newPackage(const std::string& name){
	//cout << "newPackage: " << name << endl;
	//cout << getSecond() <<endl;
}

void Listener::newClass(const ClassKey& classKey){
	//cout << "newClass: key=" << classKey << endl;
	//cout << getSecond() <<endl;
}

void Listener::newAgent(const Agent& agent){
	//cout << "newAgent: " << agent << endl;
	//cout << getSecond() << endl;
}
void Listener::delAgent(const Agent& agent){
	//cout << "delAgent: " << agent << endl;
	//cout << getSecond() << endl;
}

void Listener::objectProps(Broker& broker, Object& object){
	//discover new objects here
	//cout << "objectProps: broker=" << broker << " object=" << object << endl;
	//cout << getSecond() << endl;

	enum ObjectType t = objectStringToType(object.getClassKey().getClassName());
	ObjectInfo *oi = newObjectInfoByType(&object, &broker, t);
	if(oi == NULL)
		return;

	ListenerEvent *le = new ObjectPropertyEvent(t, oi);
	//cout << "passing event: return " << 
	ListenerEvent::sendPtr(this->eventfd, le);
	// << endl;
}

void Listener::objectStats(Broker& broker, Object& object){
	//cout << "objectStats: broker=" << broker << " object=" << object << endl;
	//cout << getSecond() <<endl;
/*
return;
	string classname = object.getClassKey().getClassName();
	struct ListenerEvent le;
	if(classname.compare("queue") !=0)
	return;
	le.type = BYTEDEQUEUE;
	le.oid = object.getObjectId();
	le.ullvalue = object.attrUint64("msgTotalDequeues");
	le.uvalue = object.attrUint("byteDepth");
	writeEvent(le);
*/
}

void Listener::sendLinkDownEvent(string srcurl, string dsturl, bool fromqmf){
	ListenerEvent *le = new LinkDownEvent(
	srcurl,
	dsturl,
	fromqmf);
	cout << "passing event: return " << ListenerEvent::sendPtr(this->eventfd, le) << endl;
}

void Listener::event(Event& event){
	//cout << event << endl;
	//cout << getSecond() <<endl;
	const string linkdownstr = "brokerLinkDown";
	const struct SchemaClass* sc = event.getSchema();
	string classname = sc->key.getClassName();
//	cout << "event: " << event << endl;

	if(classname.compare(linkdownstr) != 0){
		return;
//		cout << "event: " << classname << endl <<
//		"  exchange: " << event.attrString("exName") << endl <<
//		"  queue: " << event.attrString("qName") << endl;
	}
	cout << "link down event: " << event << "\n";
	cout << "link down event at " << getSecond() << "\n";
	//only report local route error
	this->sendLinkDownEvent(
	event.getBroker()->getUrl(),
	event.attrString("rhost"),
	true);
}


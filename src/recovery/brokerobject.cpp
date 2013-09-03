/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include<cstdio>
#include"listener.h"
#include"brokerobject.h"

#include <cstring>
#include <ctime>

#define STDCOUT(A)
//#define STDCOUT(A) std::cout << A
#define STDCOUTFLUSH()
//#define STDCOUTFLUSH() std::cout.flush()

void urlToIPPort(string url, char *ip, unsigned &port){
	unsigned i;
	for(i = 0; url[i] != ':'; i++)ip[i] = url[i];
	ip[i] = '\0';
	port = 0;
	for(i++; i < url.length(); i++)
		port = port * 10 + (url[i] - '0');
}

string IPPortToUrl(const char *ip, unsigned port){
	char s[64];
	sprintf(s, "%s:%u", ip, port);
	string r = s;
	return r;
}

static string utos(unsigned u){
	char s[16];
	sprintf(s, "%u", u);
	return (string)s;
}

string linkObjectDest(Object &o){
	return o.attrString("host") + ":" + utos(o.attrUint("port"));
}

static struct{
	enum ObjectType t;
	const char *s;
}typestring[NUMBER_OF_OBJ_TYPES]={
	{OBJ_BROKER, "broker"},
	{OBJ_LINK, "link"},
	{OBJ_EXCHANGE, "exchange"},
	{OBJ_QUEUE, "queue"},
	{OBJ_BINDING, "binding"},
	{OBJ_BRIDGE, "bridge"}
};

string objectTypeToString(enum ObjectType ty){
	int a;
	for(a = 0; a < NUMBER_OF_OBJ_TYPES; a++){
		if(typestring[a].t == ty)
			return typestring[a].s;
	}
	return "";
}

enum ObjectType objectStringToType(string st){
	int a;
	for(a = 0; a < NUMBER_OF_OBJ_TYPES; a++){
		if(st.compare((string)(typestring[a].s)) == 0)
			return typestring[a].t;
	}
	return OBJ_UNKNOWN;
}

ObjectInfo *newObjectInfoByType(Object *object, enum ObjectType t){
	Broker *broker = object->getBroker();
	switch(t){
	case OBJ_BROKER:
		return new BrokerInfo(*object, broker);
	case OBJ_LINK:
		return new LinkInfo(*object, broker);
	case OBJ_EXCHANGE:
		return new ExchangeInfo(*object, broker);
	case OBJ_QUEUE:
		return new QueueInfo(*object, broker);
	case OBJ_BINDING:
		return new BindingInfo(*object, broker);
	case OBJ_BRIDGE:
		return new BridgeInfo(*object, broker);
	default:
		return NULL;
	}
}

//class ObjectInfo and its subclasses

bool ObjectInfo::operator==(ObjectInfo &oi){
	return this->getId() == oi.getId() &&
	this->getBrokerUrl().compare(oi.getBrokerUrl()) == 0;
}

ObjectInfo::ObjectInfo(Object &obj, Broker *broker, enum ObjectType ty){
	this->type = ty;
	this->objectid = obj.getObjectId();
	this->brokerurl = obj.getBroker()->getUrl();
	//brokerptr = broker;
}

ObjectInfo::ObjectInfo(ObjectInfo *copysrc){
	this->type = copysrc->type;
	this->objectid = copysrc->objectid;
	this->brokerurl = copysrc->brokerurl;
	//this->brokerptr = copysrc->brokerptr;
}

ObjectInfo::~ObjectInfo(){	
}

BrokerInfo::BrokerInfo(Object &obj, Broker *broker):
ObjectInfo(obj, broker, OBJ_BROKER){
}

BrokerInfo::BrokerInfo(BrokerInfo *copysrc):
ObjectInfo(copysrc){
}

ObjectInfo *BrokerInfo::duplicate(){
	return new BrokerInfo(this);
}

LinkInfo::LinkInfo(Object &obj, Broker *broker):
ObjectInfo(obj, broker, OBJ_LINK){
	this->host = obj.attrString("host");
	this->port = obj.attrUint("port");
	STDCOUT("LinkInfo: " << host << ":" << port << "\n");
	STDCOUTFLUSH();
}

LinkInfo::LinkInfo(LinkInfo *copysrc):
ObjectInfo(copysrc){
	this->host = copysrc->host;
	this->port = copysrc->port;
}

ObjectInfo *LinkInfo::duplicate(){
	return new LinkInfo(this);
}

ExchangeInfo::ExchangeInfo(Object &obj, Broker *broker):
ObjectInfo(obj, broker, OBJ_EXCHANGE){
	this->name = obj.attrString("name");
	this->extype = obj.attrString("type");
 	STDCOUT("ExchangeInfo: " << name << " " << extype << "\n");
	STDCOUTFLUSH();
}

ExchangeInfo::ExchangeInfo(ExchangeInfo *copysrc):
ObjectInfo(copysrc){
	this->name = copysrc->name;
	this->extype = copysrc->extype;
}

ObjectInfo *ExchangeInfo::duplicate(){
	return new ExchangeInfo(this);
}

QueueInfo::QueueInfo(Object &obj, Broker *broker):
ObjectInfo(obj, broker, OBJ_QUEUE){
	this->name = obj.attrString("name");
	this->deqcount = 0;
	this->depth = 0;
	STDCOUT("QueueInfo: " << name << endl);
	STDCOUTFLUSH();
}

QueueInfo::QueueInfo(QueueInfo *copysrc):
ObjectInfo(copysrc){
	this->name = copysrc->name;
	this->deqcount = 0;
	this->depth = 0;
}

ObjectInfo *QueueInfo::duplicate(){
	return new QueueInfo(this);
}

BindingInfo::BindingInfo(Object &obj, Broker *broker):
ObjectInfo(obj, broker, OBJ_BINDING){
	this->bindingkey = obj.attrString("bindingKey");
	this->exid = obj.attrRef("exchangeRef");
	this->quid = obj.attrRef("queueRef");
	this->exname = "";
	this->quname = "";
	//search exname and quname when recovering
}

BindingInfo::BindingInfo(BindingInfo *copysrc):
ObjectInfo(copysrc){
	this->bindingkey = copysrc->bindingkey;
	this->exid = copysrc->exid;
	this->quid = copysrc->quid;
	this->exname = copysrc->exname;
	this->quname = copysrc->quname;
}

ObjectInfo *BindingInfo::duplicate(){
	return new BindingInfo(this);
}

BridgeInfo::BridgeInfo(Object &obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_BRIDGE){
	this->linkid = obj.attrRef("linkRef");
	this->src = obj.attrString("src");
	this->dest = obj.attrString("dest");
	this->routingkey = obj.attrString("key");
	this->isqueue = obj.attrBool("srcIsQueue");
	this->isdynamic = obj.attrBool("dynamic");
	STDCOUT("BridgeInfo: " << linkid << " " << src << " " << dest << " " << routingkey <<
	(isqueue? " q": "") << (isdynamic? " d\n": "\n"));
	STDCOUT(*(obj.getBroker()) << endl);
	STDCOUTFLUSH();
}

BridgeInfo::BridgeInfo(BridgeInfo *copysrc):
ObjectInfo(copysrc){
	this->linkid = copysrc->linkid;
	this->src = copysrc->src;
	this->dest = copysrc->dest;
	this->routingkey = copysrc->routingkey;
	this->isqueue = copysrc->isqueue;
	this->isdynamic = copysrc->isdynamic;
}

ObjectInfo *BridgeInfo::duplicate(){
	return new BridgeInfo(this);
}

string ExchangeInfo::getName(){
	return name;
}

string QueueInfo::getName(){
	return name;
}

void QueueInfo::setQueueStat(unsigned long long &count, unsigned &dep){
	unsigned long long lastdeqcount = this->deqcount;
	unsigned lastdepth = this->depth;
	this->deqcount = count;
	this->depth = dep;
	count = lastdeqcount;
	dep = lastdepth;
}

int BindingInfo::setBindingNames(
vector<ObjectInfo*> &exvec, vector<ObjectInfo*> &quvec){
	int ret=0;
	for(unsigned i = 0; i < exvec.size(); i++){
		if(exvec[i]->getId() == exid &&
		exvec[i]->getBrokerUrl().compare(this->getBrokerUrl()) == 0){
			exname = ((ExchangeInfo*)(exvec[i]))->getName();
			ret++;
			break;
		}
	}
	for(unsigned i = 0; i < quvec.size(); i++){
		if(quvec[i]->getId() == quid &&
		quvec[i]->getBrokerUrl().compare(this->getBrokerUrl()) == 0){
			quname = ((QueueInfo*)(quvec[i]))->getName();
			ret++;
			break;
		}
	}
	STDCOUT("binding: " << bindingkey << " " << exname << " " << quname << "\n");
	return (ret==2?1:0);
}

string LinkInfo::getLinkDestHost(){
	return host;
}
unsigned int LinkInfo::getLinkDestPort(){
	return port;
}

string LinkInfo::getLinkDestUrl(){
	return host + ":" + utos(port);
}

ObjectId BridgeInfo::getLinkId(){
	return linkid;
}

ObjectId ObjectInfo::getId(){
	return objectid;
}

string ObjectInfo::getBrokerUrl(){
	return brokerurl;
}
/*
Broker *ObjectInfo::getBrokerPtr(){
	return brokerptr;
}
*/
int BrokerInfo::copyTo(Object *destobj){
	cerr << "error: Broker::copyTo()\n";
	return -1;
}

int LinkInfo::copyTo(Object *destobj){
	return copyToNewDest(destobj, host, port);
}

int LinkInfo::copyToNewDest(Object *destobj, string newhost, unsigned newport){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addLink(newhost, newport);
}

int ExchangeInfo::copyTo(Object *destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addExchange(name, extype);
}

int QueueInfo::copyTo(Object *destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addQueue(name);
}

int BindingInfo::copyTo(Object *destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addBinding(bindingkey, exname, quname);
}

int BridgeInfo::copyTo(Object *destobj){
	ObjectOperator destoper(destobj, OBJ_LINK);
	return destoper.addBridge(src, dest, routingkey, isqueue, isdynamic);
}

//class ObjectOperator
ObjectOperator::ObjectOperator(Object *o, enum ObjectType t){
    setObject(o, t);
}
void ObjectOperator::setObject(Object *o, enum ObjectType t){
    object = o;
    type = t;
}
//broker operation
int ObjectOperator::addExchange(string name, string extype,
int durable, string alternate){
    if(this->type != OBJ_BROKER)
        return -1;
    Object::AttributeMap args;
    args.addString("type", "exchange");
    args.addString("name", name);
    qpid::framing::FieldTable ft;
    ft.setString("exchange-type", extype);
    ft.setInt("durable", durable);
    if(alternate.compare("") != 0)
        ft.setString("alternate-exchange", alternate);
    args.addMap("properties", ft);
    args.addBool("strict", true); //check unrecognized options
    struct MethodResponse mr;
    object->invokeMethod("create", args, mr);
    return mr.code;
}
int ObjectOperator::addQueue(string name, int autodelete,
int durable, string alternate){
    if(this->type != OBJ_BROKER)
        return -1;
    Object::AttributeMap args;
    args.addString("type", "queue");
    args.addString("name", name);
    qpid::framing::FieldTable ft;
    ft.setInt("durable", durable);
    ft.setInt("auto-delete", autodelete);
    if(alternate.compare("") != 0)
        ft.setString("alternate-exchange", alternate);
    args.addMap("properties", ft);
    args.addBool("strict", true);
    struct MethodResponse mr;
    object->invokeMethod("create", args, mr);
    return mr.code;
}
int ObjectOperator::addBinding(string key, string exname, string quname){
    if(this->type != OBJ_BROKER)
        return -1;
    Object::AttributeMap args;
    args.addString("type", "binding");
    args.addString("name", exname + "/" + quname + "/" + key);
    qpid::framing::FieldTable ft;
    args.addMap("properties", ft);
    args.addBool("strict", true);
    struct MethodResponse mr;
    object->invokeMethod("create", args, mr);
    return mr.code;
    return -1;
}
int ObjectOperator::addLink(string host, unsigned port,
string username, string password,
bool durable, string auth, string transport){
    if(this->type != OBJ_BROKER)
        return -1;
    Object::AttributeMap args;
    args.addString("host", host);
    args.addUint("port", port);
    args.addBool("durable", durable);
    args.addString("username", username);
    args.addString("password", password);
    args.addString("authMechanism", auth);
    args.addString("transport", transport);
    struct MethodResponse mr;
    object->invokeMethod("connect", args, mr);
    return mr.code;
}
//link operation
int ObjectOperator::addBridge(string src, string dest, string key,
bool isqueue, bool isdynamic,
bool durable, string tag, string excludes,
bool islocal, int issync){
    if(this->type != OBJ_LINK)
        return -1;
    Object::AttributeMap args;
    args.addString("src", src);// exchange name
    args.addString("dest", dest);
    args.addString("key", key);
    args.addBool("durable", durable);
    args.addString("tag", tag);
    args.addString("excludes", excludes);
    args.addBool("srcIsLocal", islocal);
    args.addBool("srcIsQueue", isqueue);
    args.addBool("dynamic", isdynamic);
    args.addUint("sync", issync);
    struct MethodResponse mr;
    object->invokeMethod("bridge", args, mr);
    return mr.code;
}

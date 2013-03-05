#include<cstdio>
#include"listener.h"
#include"brokerobject.h"

#include <cstring>
#include <ctime>

static string utos(unsigned u){
	char s[16];
	sprintf(s, "%u", u);
	return (string)s;
}

string linkObjectDest(Object& o){
	return o.attrString("host")+":"+utos(o.attrUint("port"));
}

struct ObjectTypeString{
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
	for(a=0; a<NUMBER_OF_OBJ_TYPES; a++){
		if(typestring[a].t==ty)
			return typestring[a].s;
	}
	return "";
}

enum ObjectType objectStringToType(string st){
	int a;
	for(a=0; a<NUMBER_OF_OBJ_TYPES; a++){
		if(st.compare((string)(typestring[a].s))==0)
			return typestring[a].t;
	}
	return OBJ_UNKNOWN;
}

ObjectInfo*newObjectInfoByType(Object& object, Broker*broker, enum ObjectType t){
	switch(t){
	case OBJ_BROKER:
		return new BrokerInfo(object, broker);
	case OBJ_LINK:
		return new LinkInfo(object, broker);
	case OBJ_EXCHANGE:
		return new ExchangeInfo(object, broker);
	case OBJ_QUEUE:
		return new QueueInfo(object, broker);
	case OBJ_BINDING:
		return new BindingInfo(object, broker);
	case OBJ_BRIDGE:
		return new BridgeInfo(object, broker);
	default:
		return NULL;
	}
}

//class ObjectInfo and its subclasses

bool ObjectInfo::operator==(ObjectInfo& oi){
	return this->getId() == oi.getId() &&
	this->getBrokerUrl().compare(oi.getBrokerUrl()) == 0;
}

void BrokerInfo::readArgs(Object& obj){
}

void LinkInfo::readArgs(Object& obj){
	host = obj.attrString("host");
	port = obj.attrUint("port");
	cout << "LinkInfo: "<< host << ":" << port << "\n";
	cout.flush();
}

void QueueInfo::readArgs(Object& obj){
	name = obj.attrString("name");
	deqcount = 0;
	depth = 0;
	cout << "QueueInfo: " << name << endl;
	cout.flush();
}

void ExchangeInfo::readArgs(Object& obj){
	name = obj.attrString("name");
	extype = obj.attrString("type");
 	cout << "ExchangeInfo: " << name << " " << extype << "\n";
	cout.flush();
}

void BindingInfo::readArgs(Object& obj){
	bindingkey = obj.attrString("bindingKey");
	exid = obj.attrRef("exchangeRef");
	quid = obj.attrRef("queueRef");
	exname = "";
	quname = "";
	//search exname and quname when recovering
}

void BridgeInfo::readArgs(Object& obj){
	linkid = obj.attrRef("linkRef");
	src = obj.attrString("src");
	dest = obj.attrString("dest");
	routingkey = obj.attrString("key");
	isqueue = obj.attrBool("srcIsQueue");
	isdynamic = obj.attrBool("dynamic");
	cout << "BridgeInfo: " << linkid << " " << src << " " << dest << " " << routingkey
	<< (isqueue? " q": "") << (isdynamic? " d\n": "\n");
cout<< *(obj.getBroker()) << endl;
	cout.flush();
}

ObjectInfo::ObjectInfo(Object& obj, Broker*broker, enum ObjectType ty){
	init(obj, broker, ty);
}

void ObjectInfo::init(Object& obj, Broker*broker, enum ObjectType ty){
	type = ty;
	objectid = obj.getObjectId();
	brokerurl = obj.getBroker()->getUrl();
	brokerptr = broker;
}

BrokerInfo::BrokerInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_BROKER){
	readArgs(obj);
}

LinkInfo::LinkInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_LINK){
	readArgs(obj);
}

ExchangeInfo::ExchangeInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_EXCHANGE){
	readArgs(obj);
}

QueueInfo::QueueInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_QUEUE){
	readArgs(obj);
}

BindingInfo::BindingInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_BINDING){
	readArgs(obj);
}

BridgeInfo::BridgeInfo(Object& obj, Broker*broker):
ObjectInfo(obj, broker, OBJ_BRIDGE){
	readArgs(obj);
}

string ExchangeInfo::getName(){
	return name;
}

string QueueInfo::getName(){
	return name;
}

void QueueInfo::setQueueStat(unsigned long long& count, unsigned& dep){
	unsigned long long lastdeqcount = deqcount;
	unsigned lastdepth = depth;
	deqcount = count;
	depth = dep;
	count = lastdeqcount;
	dep = lastdepth;
}

int BindingInfo::setBindingNames(
vector<ObjectInfo*>& exvec, vector<ObjectInfo*>& quvec){
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
	cout << "binding: " << bindingkey << " " << exname << " " << quname << "\n";
	return (ret==2?1:0);
}

string LinkInfo::getLinkDestHost(){
	return host;
}
unsigned int LinkInfo::getLinkDestPort(){
	return port;
}

string LinkInfo::getLinkDestUrl(){
	return host+":"+utos(port);
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
string BridgeInfo::getBridgeSrcUrl(){
	return src;
}
string BridgeInfo::getBridgeDestUrl(){
	return dest;
}

int BrokerInfo::copyTo(Object* destobj){
	cerr << "error: Broker::copyTo()\n";
	return -1;
}

int LinkInfo::copyTo(Object* destobj){
	return copyToNewDest(destobj, host, port);
}

int LinkInfo::copyToNewDest(Object* destobj, string newhost, unsigned newport){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addLink(newhost, newport);
}

int ExchangeInfo::copyTo(Object* destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addExchange(name, extype);
}

int QueueInfo::copyTo(Object* destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addQueue(name);
}

int BindingInfo::copyTo(Object* destobj){
	ObjectOperator destoper(destobj, OBJ_BROKER);
	return destoper.addBinding(bindingkey, exname, quname);
}

int BridgeInfo::copyTo(Object* destobj){
	ObjectOperator destoper(destobj, OBJ_LINK);
	return destoper.addBridge(src, dest, routingkey, isqueue, isdynamic);
}

//class SystemInfo
SystemInfo::SystemInfo(/*ObjectId oid, */const char* ipaddr){
    // objectid = oid;
    totaldeq = lasttotaldeq = 0;
    totaldepth = 0;
    deqrate = 0;
    hoptable = NULL;
    nhoptable = 0;
    ttlvalue = large_ttl;
    strcpy(ipaddress, ipaddr);
}

SystemInfo::~SystemInfo(){
    if(hoptable != NULL)
        delete[] hoptable;
}
/*
ObjectId SystemInfo::getId(){
    return objectid;
}
*/
string SystemInfo::getAddress(){
    return ipaddress;
}

int SystemInfo::getDefaultTTL(){
    return ttlvalue;
}

void SystemInfo::addStatistics(unsigned long long countchange, long long depthchange){
    totaldeq += countchange;
    totaldepth += depthchange;
}

void SystemInfo::updateDequeueRate(double interval){
    const double alpha = 0.5;
    deqrate = (1 - alpha) * deqrate +
               alpha * (totaldeq - lasttotaldeq) / interval;
    lasttotaldeq = totaldeq;
}

double SystemInfo::getDequeueRate(){
    if(totaldepth == 0)
        return 1e50;
    return deqrate;
}

void SystemInfo::initPingdData(vector<SystemInfo>& systemvec, vector<SystemInfo>& outsystemvec){
    if(hoptable != NULL)
        delete[] hoptable;
    const int svsize = systemvec.size();
    nhoptable = svsize + outsystemvec.size();
    hoptable = new PingdData[nhoptable];

    for(int i = 0; i < nhoptable; i++){
        hoptable[i].destip = (i >= svsize? outsystemvec[i-svsize].getAddress(): systemvec[i].getAddress());
        hoptable[i].ttl = large_ttl;
        hoptable[i].lastreturn = 1;
    }
}

void SystemInfo::initPingdData(SystemInfo& sysinfo){ // only for outside node
    if(hoptable != NULL)
        delete[] hoptable;
    nhoptable = 1;
    hoptable = new PingdData[nhoptable];

    hoptable[0].destip = sysinfo.getAddress();
    hoptable[0].ttl = large_ttl;
    hoptable[0].lastreturn = 1;
}

int SystemInfo::getTTL(string destip, int& ttl){
    const time_t expiretime = 2; // seconds

    if(destip.compare("127.0.0.1") == 0 || destip.compare("") == 0){
        ttl = this->ttlvalue;
        return 0;
    }

    for(int i = 0; i < nhoptable; i++){
        if(hoptable[i].destip.compare(destip) != 0)
            continue;
        ttl = hoptable[i].ttl;
        return (time(NULL) - hoptable[i].lastreturn > expiretime);
    }
    return -1;
}

int SystemInfo::setTTL(string destip, int ttl, time_t returntime){

    if(destip.compare("127.0.0.1") == 0 || destip.compare("") == 0){
        this->ttlvalue = ttl;
        return 0;
    }

    for(int i = 0; i < nhoptable; i++){
        if(hoptable[i].destip.compare(destip) != 0)
            continue;
        hoptable[i].ttl = ttl;
        hoptable[i].lastreturn = returntime;
        return 0;
    }
    return -1;
}

/*class BrokerInfo
vector<ObjectInfo>* BrokerInfo::getVector(enum ObjectType t, bool in){
    switch(t){
    case OBJ_EXCHANGE:
        return &exchanges;
    case OBJ_QUEUE:
        return &queues;
    case OBJ_BINDING:
        return &bindings;
    case OBJ_LINK:
        return in? &inlinks: &links;
    case OBJ_BRIDGE:
        return &bridges;
    default:
        return NULL;
    }
}

BrokerInfo::BrokerInfo(string host, unsigned port, Broker* b, SystemInfo* sysptr){
    char s[16];
    string s2;
    sprintf(s, "%u",port);
    init(host + ":" + (string)s, b, sysptr);
}
BrokerInfo::BrokerInfo(string hostport, Broker* b, SystemInfo* sysptr){
    init(hostport, b, sysptr);
}
void BrokerInfo::init(string url, Broker* b, SystemInfo* sysptr){
    ObjectInfo::init(OBJ_BROKER, NULL);
    brokerPtr = b;
    setBrokerUrl(url);
    deleted = false;
    exchanges.clear();
    queues.clear();
    bindings.clear();
    links.clear();
    bridges.clear();
    inlinks.clear();
    systeminfo = sysptr;
}
Broker* BrokerInfo::getBroker(){
    return brokerPtr;
}
void BrokerInfo::initVector(enum ObjectType t, Object::Vector& list){
    vector<ObjectInfo>& v = *(getVector(t, false));
    v.resize(list.size());
    for(unsigned i = 0; i != list.size(); i++)
        v[i].init(t, list[i]);
}
void BrokerInfo::insertVector(ObjectType t, ObjectInfo& obj, bool in){
    vector<ObjectInfo>& v = *(getVector(t, in));
    v.push_back(obj);
}
void BrokerInfo::initBindings(){
    for(unsigned i = 0; i < bindings.size(); i++){
        bindings[i].initBinding(exchanges, queues);
    }
}
void BrokerInfo::initInLinks(vector<BrokerInfo>& brokervec){
    for(unsigned i =0; i < links.size(); i++){
        string url = links[i].getLinkDest();
        for(unsigned j = 0; j < brokervec.size() ; j++){
            if(brokervec[j].getBrokerUrl() == url){
                brokervec[j].insertVector(OBJ_LINK, links[i], true);
cout << "link(to): " << url << " to " <<  getBrokerUrl() << endl;
                break;
            }
        }
    }
}

void BrokerInfo::setDeleted(bool d){
    deleted = d;
}

void BrokerInfo::copyObjectsToBroker(BrokerInfo* bi, Object* b){
    vector<ObjectInfo>* v[4]={
        &exchanges,
        &queues,
        &bindings,
        &links
    };

    for(unsigned i = 0; i < 4; i++){
        vector<ObjectInfo>& v2 = *(v[i]);
        for(unsigned j = 0; j < v2.size(); j++){
            v2[j].copyTo(b);
        }
    }
}

void BrokerInfo::copyBridges(BrokerInfo* rinfo, Object::Vector& list){
    for(unsigned i = 0; i != bridges.size(); i++){
        ObjectId linkobjid = bridges[i].getLinkId();
        string desturl = "";
        for(unsigned j = 0; j < links.size(); j++){
            if(links[j].getId() != linkobjid)
                continue;
            desturl = links[j].getLinkDest();
            break;
        }
        if(desturl.compare("") == 0)
            cout << "error: cannot find link";

        for(unsigned j = 0; j < list.size(); j++){
            string host = list[j].attrString("host");
            unsigned port = list[j].attrUint("port");
            char p[16];
            sprintf(p, "%u", port);
            string linkurl = host + ":" + (string)p;
            if(desturl.compare(linkurl) != 0)
                continue;
            bridges[i].copyTo(&(list[j]));
        }
    }
}

void BrokerInfo::redirectInLinks(BrokerInfo* rinfo, Object::Vector& list, vector<BrokerInfo>& brokervec){
    for(unsigned i = 0;i < inlinks.size(); i++){
        string burl = inlinks[i].getBrokerUrl();
        unsigned sepa = burl.find(':');
        string host = burl.substr(0, sepa);
        unsigned port = 0;
        for(unsigned j = sepa+1; j < burl.size();j++)
            port = port * 10 + (burl[j] - '0');

        for(unsigned j = 0; j < list.size(); j++){
            Broker* b = list[j].getBroker();
            if(burl.compare(b->getUrl()) != 0)
                continue;
            ObjectOperator boper(&(list[j]), OBJ_BROKER);
            boper.addLink(host,port);
            break;
        }
    }
}

void BrokerInfo::updateQueueStat(ObjectId oid, unsigned long long count, unsigned dep){
    unsigned long long lastcount = count;
    unsigned lastdep = dep;
    for(unsigned i = 0; i != queues.size(); i++){
        if(queues[i].getId() != oid)
            continue;
        queues[i].setQueueStat(lastcount, lastdep);
        this->systeminfo->addStatistics(
        count - lastcount,
        ((signed long long)dep) - ((signed long long)lastdep)
        );
        return;
    }
    cerr << "error: updateQueueStat\n";
}

void BrokerInfo::getLinkHostList(vector<string>& hostlist){
    hostlist.resize(links.size());
    for(unsigned i = 0; i < links.size(); i++)
        hostlist[i]=links[i].getLinkDestAddress();
}
*/

//class ObjectOperator
ObjectOperator::ObjectOperator(Object* o, enum ObjectType t){
    setObject(o, t);
}
void ObjectOperator::setObject(Object* o, enum ObjectType t){
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
    args.addString("src", src);//will src and dest be overwritten?
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


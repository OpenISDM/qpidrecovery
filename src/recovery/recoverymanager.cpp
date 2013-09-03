/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include"recoverymanager.h"
#include<unistd.h>
#include<iostream>
#include"timestamp.h"
#include"messagecopy.h"
#include"namelist.h"

using namespace std;

int RecoveryManager::getBrokerIndexByAddress(BrokerAddress &ba){
	for(unsigned i = 0; i < this->bavec.size(); i++){
		if(this->bavec[i].address == ba)
			return (int)i;
	}
	return -1;
}

static bool isIgnoredName(string str){
	const char *pattern[]={
		"qmf.","qmfc-","qpid.","amq.",NULL
	};
	if(str.size() == 0 || str.size() >= 20)
		return true;
	for(unsigned j = 0; pattern[j] != NULL; j++){
		bool matchflag = true;
		for(unsigned i=0; pattern[j][i] != '\0'; i++){
			if(i == str.size()){
				matchflag = false;
				break;
			}
			if(str[i] != pattern[j][i]){
				matchflag = false;
				break;
			}
		}
		if(matchflag == true){
			return true;
		}
	}
	return false;
}

int RecoveryManager::addObjectInfo(ObjectInfo* objinfo, enum ObjectType objtype){
	ObjectInfoPtrVec *vec;

	switch(objtype){
	case OBJ_BROKER:
		vec = &(this->brokers);
		break;
	case OBJ_EXCHANGE:
		if(isIgnoredName(((ExchangeInfo*)objinfo)->getName()))
			return 1;
		vec = &(this->exchanges);
		break;
	case OBJ_BINDING:
		vec = &(this->bindings);
		break;
	case OBJ_QUEUE:
		if(isIgnoredName(((QueueInfo*)objinfo)->getName()))
			return 1;
		vec = &(this->queues);
		break;
	case OBJ_BRIDGE:
		vec = &(this->bridges);
		break;
	case OBJ_LINK:
		vec = &(this->links);
		break;
	default:
		cerr << "addNewObject error\n";
		return 1;
	}

	for(ObjectInfoPtrVec::iterator i = vec->begin(); i < vec->end(); i++){
		if(**i == *objinfo){// compare id && brokerurl
			delete (*i);
			*i = objinfo;
			return 0;
		}
	}
	vec->push_back(objinfo);
	return 0;
}

class AddBrokerArgument{
public:
	RecoveryManager *recovery;
	BrokerAddress address;

	AddBrokerArgument(RecoveryManager *rm, BrokerAddress &ba){
		this->recovery = rm;
		this->address = ba;
	}
};

void *addBrokerThread(void *voidab){
	AddBrokerArgument *abarg = (AddBrokerArgument*)voidab;

	qpid::client::ConnectionSettings settings;
	settings.host = abarg->address.ip;
	settings.port = abarg->address.port;
	Broker *broker = abarg->recovery->sm->addBroker(settings);
	broker->waitForStable();
	struct BrokerAddressPair bapair;
	bapair.address = abarg->address;
	bapair.pointer = broker;
	{
		pthread_mutex_lock(&(abarg->recovery->bavecmutex));
		abarg->recovery->bavec.push_back(bapair);
		pthread_mutex_unlock(&(abarg->recovery->bavecmutex));
	}

cout << "recoverymanager: addBroker " << abarg->address.ip << ":" << abarg->address.port << endl;
	Object::Vector objvec;
	for(int j = 0; j != NUMBER_OF_OBJ_TYPES; j++){
		enum ObjectType t = (enum ObjectType)j;
		objvec.clear();
		abarg->recovery->sm->getObjects(objvec, objectTypeToString(t), broker);
		for(Object::Vector::iterator iter = objvec.begin(); iter != objvec.end(); iter++){
			abarg->recovery->listener->objectProps(*broker, *iter);
/*			ObjectInfo *objinfo;
			objinfo = newObjectInfoByType(&(objvec[i]), t);
			if(objinfo != NULL)
				abarg->recovery->addObjectInfo(objinfo, t);*/
		}
	}
	logTime("brokerAdded");

	delete abarg;
	return NULL;
}

int RecoveryManager::addBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);
	int brokerindex;
cout << "recoverymanager: prepare addBroker " << ip << ":" << port << "\n";
	{
		pthread_mutex_lock(&(this->bavecmutex));
		brokerindex = this->getBrokerIndexByAddress(ba);
		pthread_mutex_unlock(&(this->bavecmutex));
	}
	if(brokerindex >= 0){
		return 0;
	}
	AddBrokerArgument *abarg = new AddBrokerArgument(this, ba);
	pthread_t threadid;
	pthread_create(&threadid, NULL, addBrokerThread, abarg);
	//pthread_detach(threadid);
	void *threadreturn;
	pthread_join(threadid, &threadreturn);

	return 0;
}

int RecoveryManager::deleteBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);
	//Broker *broker = NULL;
	{
		pthread_mutex_lock(&(this->bavecmutex));
		int i = this->getBrokerIndexByAddress(ba);
		if(i != -1){
	//		broker = this->bavec[i].pointer;
			this->bavec.erase(this->bavec.begin() + i);	
		}
		pthread_mutex_unlock(&(this->bavecmutex));
	}
	//TODO: this operation sometimes blocks
	//this->sm->delBroker(broker);
	return 0;
}

static int getIndexByUrlObjectId(vector<ObjectInfo*> &objs, string url, ObjectId &oid){
	for(unsigned i = 0; i != objs.size(); i++){
		if(oid == objs[i]->getId() && url.compare(objs[i]->getBrokerUrl()) == 0)
			return (signed)i;
	}
	return -1;
}

static void copyBridges(ObjectInfoPtrVec &links, ObjectInfoPtrVec &bridges,
Object::Vector &linkobjlist,
string oldsrc, string newsrc,
bool changedst = false, string olddst = "", string newdst = ""){
	int copycount=0;
	cout << bridges.size() << " bridges in total\n";
	for(ObjectInfoPtrVec::iterator i = bridges.begin(); i != bridges.end(); i++){
		string src = (*i)->getBrokerUrl();
		if(oldsrc.compare(src) != 0)
			continue;

		string dst;
		{// find dst
			ObjectId oldlinkid = ((BridgeInfo*)(*i))->getLinkId();
			int j = getIndexByUrlObjectId(links, src, oldlinkid);
			if(j < 0){
				cerr << "link id & broker url not found\n";
				continue;
			}
			dst = ((LinkInfo*)(links[j]))->getLinkDestUrl();
		}
		if(changedst == true && olddst.compare(dst) != 0)
			continue;
		// (*i) is a bridge from oldsrc to dst
		cout << src << "->" << dst << "\n";
		for(Object::Vector::iterator j = linkobjlist.begin(); j != linkobjlist.end(); j++){
			if(
			(j->getBroker())->getUrl().compare(newsrc) == 0 &&
			linkObjectDest(*j).compare(changedst == true? newdst: dst) == 0){
				(*i)->copyTo(&(*j));
				copycount++;
				break;
			}
		}
	}
	cout << copycount << " bridges copied\n";
}

class CopyObjectsArgs{
public:
	ObjectInfoPtrVec exchanges, queues, bindings, brokers, links, bridges;
	Listener *listener;
	char failip[32];
	unsigned failport;
	char backupip[32];
	unsigned backupport;

private:
	void copyVec(ObjectInfoPtrVec &from, ObjectInfoPtrVec &to, string url){
		to.clear();
		for(ObjectInfoPtrVec::iterator i = from.begin(); i != from.end(); i++){
			if(url.compare((*i)->getBrokerUrl()) != 0){
					continue;
			}
			to.push_back((*i)->duplicate());
		}
	}

	void deleteVec(ObjectInfoPtrVec &v){
			for(ObjectInfoPtrVec::iterator i = v.begin(); i != v.end(); i++){
				delete (*i);
			}
			v.clear();
	}

public:
	CopyObjectsArgs(
	ObjectInfoPtrVec &cpexchanges,
	ObjectInfoPtrVec &cpqueues,
	ObjectInfoPtrVec &cpbindings,
	ObjectInfoPtrVec &cpbrokers,
	ObjectInfoPtrVec &cplinks,
	ObjectInfoPtrVec &cpbridges,
	Listener *cplistener,
 	const char *cpfailip, unsigned cpfailport,
	const char *cpbackupip, unsigned cpbackupport
	){
		string failurl = IPPortToUrl(cpfailip, cpfailport);
		copyVec(cpexchanges, exchanges, failurl);
		copyVec(cpqueues, queues, failurl);
		copyVec(cpbindings, bindings, failurl);
		copyVec(cpbrokers, brokers, failurl);
		copyVec(cplinks, links, failurl);
		copyVec(cpbridges, bridges, failurl);

		this->listener = cplistener;
		strcpy(failip, cpfailip);
		failport = cpfailport;
		strcpy(backupip, cpbackupip);
		backupport = cpbackupport;
	}

	~CopyObjectsArgs(){
		deleteVec(exchanges);
		deleteVec(queues);
		deleteVec(bindings);
		deleteVec(brokers);
		deleteVec(links);
		deleteVec(bridges);
	}
};

static void *copyObjectsThread(void *voidcoarg){
	CopyObjectsArgs *coarg = (CopyObjectsArgs*)voidcoarg;

	BrokerAddress oldba(coarg->failip, coarg->failport);
	BrokerAddress newba(coarg->backupip, coarg->backupport);
	string newurl = newba.getAddress(), oldurl = oldba.getAddress();
	cout << "failure: " << oldurl << "\n";
	cout << "replace: " << newurl << "\n";
	cout << "start recovery!\n";

	int copycount;
	Object::Vector backupbrokerobjlist;
	Broker *backupbroker;
	SessionManager backupsm;
	{
		qpid::client::ConnectionSettings settings;
		settings.host = newba.ip;
		settings.port = newba.port;
		backupbroker = backupsm.addBroker(settings);
		backupbroker->waitForStable();
		backupsm.getObjects(backupbrokerobjlist, "broker", backupbroker);
	}
	if(backupbrokerobjlist.size() != 1){
		cerr << "error: add backup broker\n";
		return NULL;
	}
	Object &backupbrokerobj = (backupbrokerobjlist[0]);

	{
		ObjectInfoPtrVec *vecs[4] =
		{&(coarg->exchanges), &(coarg->queues), &(coarg->bindings), &(coarg->links)};

		for(unsigned i = 0; i < 4; i++){
			ObjectInfoPtrVec &v = *(vecs[i]);
			copycount=0;
			cout << v.size() <<" objects(" <<i << ") in total\n";
			for(ObjectInfoPtrVec::iterator j = v.begin(); j != v.end(); j++){
				if(oldurl.compare((*j)->getBrokerUrl()) != 0)
					continue;
				if(i == 2){ //binding
					if(((BindingInfo*)(*j))->setBindingNames(
					coarg->exchanges, coarg->queues) == 0)
						continue;
				}
				if(i == 3){ //link
					string linkdest=((LinkInfo*)(*j))->getLinkDestUrl();
					char dstip[64];
					unsigned dstport;
					urlToIPPort(linkdest, dstip, dstport);
					if(checkFailure(dstip) == true){
						// if the dest is down, no LinkDownEvent will come
						coarg->listener->sendLinkDownEvent(newurl, linkdest, false);
						//continue;
					}
				}
				(*j)->copyTo(&backupbrokerobj);
				copycount++;
			}
			cout << copycount << " objects copied\n";
		}
	}

	cout.flush();
	//backupsm has received link objects. now copy bridges

	Object::Vector objlist;
	backupsm.getObjects(objlist, "link", backupbroker);
	copyBridges(coarg->links, coarg->bridges, objlist, oldurl, newurl);

	backupsm.delBroker(backupbroker);

std::cout<< "recovery ends at " << getSecond() << "\n";
std::cout.flush();
logTime("finishRecovery");

	delete coarg;//new in startRecovery
	return NULL;
}

int RecoveryManager::copyObjects(const char *failip, unsigned failport,
const char *backupip, unsigned backupport){
	CopyObjectsArgs *coarg = new CopyObjectsArgs(
		this->exchanges,
		this->queues,
		this->bindings,
		this->brokers,
		this->links,
		this->bridges,
		this->listener,
		failip, failport,
		backupip, backupport
	);

	pthread_t threadid;
	pthread_create(&threadid, NULL, copyObjectsThread, coarg);
	//pthread_detach(threadid);
	void *threadreturn;
	pthread_join(threadid, &threadreturn);
	return 0;
}

int RecoveryManager::reroute(const char *srcip, unsigned srcport,
const char *oldip, unsigned oldport, const char *newip, unsigned newport){
	string srcurl = IPPortToUrl(srcip, srcport);
	string newurl = IPPortToUrl(newip, newport);
	string oldurl = IPPortToUrl(oldip, oldport);
	// find link from src to old
	ObjectInfoPtrVec::iterator linkiter;

	this->addBroker(srcip, srcport); // src is alive
	
	for(linkiter = this->links.begin(); linkiter != this->links.end(); linkiter++){
		string linksrc = ((LinkInfo*)(*linkiter))->getBrokerUrl();
		string linkdst = ((LinkInfo*)(*linkiter))->getLinkDestUrl();
		if(srcurl.compare(linksrc) == 0 && oldurl.compare(linkdst) == 0)
			break;
	}
	if(linkiter == this->links.end()){
		cout << "reroute: old link not found\n";
		return -1;
	}
	cout << "old LinkInfo found\n";

	Object::Vector linkobjlist;
	{// find broker object
		BrokerAddress ba((string)srcip, srcport);
		Broker* srcbroker = NULL;
		{
			pthread_mutex_lock(&(this->bavecmutex));
			int i = this->getBrokerIndexByAddress(ba);
			if(i != -1){
				srcbroker = this->bavec[i].pointer;
			}
			pthread_mutex_unlock(&(this->bavecmutex));
		}
		if(srcbroker == NULL)
			return -1;

		Object::Vector srcbrokerobjlist;
		srcbrokerobjlist.clear();
		this->sm->getObjects(srcbrokerobjlist, "broker", srcbroker);
		if(srcbrokerobjlist.size() != 1)
			cerr << "reroute error: source broker object list\n";
		((LinkInfo*)(*linkiter))->copyToNewDest(&(srcbrokerobjlist[0]), newip, newport);	
		this->sm->getObjects(linkobjlist, "link", srcbroker);
	}
	cout << "New link created\n";

	copyBridges(this->links, this->bridges,
	linkobjlist, srcurl, srcurl, true, oldurl, newurl);

	return 0;
}

int RecoveryManager::getEventFD(){
	return this->eventpipe[0];
}

ListenerEvent *RecoveryManager::handleEvent(){
	ListenerEvent *le = ListenerEvent::receiveEventPtr(this->eventpipe[0]);

	switch(le->getType()){
	case OBJECTPROPERTY:
		{
			ObjectPropertyEvent *ope = (ObjectPropertyEvent*)le;
			this->addObjectInfo(ope->objinfo->duplicate(), ope->objtype);
		}
		break;
	case BROKERDISCONNECTION:
		break;
	case LINKDOWN:
		break;
	}
	return le;
}

RecoveryManager::RecoveryManager(){
	pipe(eventpipe);
	listener = new Listener(eventpipe[1]);
	sm = new SessionManager(listener);
	exchanges.clear();
	queues.clear();
	bindings.clear();
	brokers.clear();
	links.clear();
	bridges.clear();
	bavec.clear();
	pthread_mutex_init(&(this->bavecmutex), NULL);
}

RecoveryManager::~RecoveryManager(){
	pthread_mutex_destroy(&(this->bavecmutex));
	delete sm;
	delete listener;
	close(eventpipe[0]);
	close(eventpipe[1]);
	
}

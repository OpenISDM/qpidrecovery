#include"recoverymanager.h"
#include<unistd.h>
#include<iostream>
#include"timestamp.h"
#include"messagecopy.h"
#include"discovery.h"

using namespace std;

int RecoveryManager::getBrokerIndexByAddress(BrokerAddress& ba){
	for(unsigned i = 0; i < this->bavec.size(); i++){
		if(this->bavec[i].address == ba)
			return (int)i;
	}
	return -1;
}

/*
int RecoveryManager::getBrokerIndexByUrl(string url){
	for(unsigned i = 0; i < this->bavec.size(); i++){
		if(url.compare(this->bavec[i].pointer->getUrl()) == 0)
			return (int)i;
	}
	return -1;
}
*/

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
			// cout << str << " is ignored\n";
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
		cerr<<"addNewObject error\n";
		return 1;
	}
	for(unsigned i = 0; i < vec->size(); i++){
		if(*((*vec)[i]) == *objinfo){// compare id && brokerurl
			//delete ((*vec)[i]);
			// improve: auto delete?
			vec->erase(vec->begin() + i);
			break;
		}
	}
	vec->push_back(objinfo);
	return 0;
}

int RecoveryManager::addBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);
	Broker *broker;
	int brokerindex;
cout << "recoverymanager: prepare addBroker " << ip << ":" << port;
	if((brokerindex = this->getBrokerIndexByAddress(ba)) < 0){
		qpid::client::ConnectionSettings settings;
		settings.host = ba.ip;
		settings.port = ba.port;
		broker = this->sm->addBroker(settings);
		broker->waitForStable();
		struct BrokerAddressPair bapair;
		bapair.address = ba;
		bapair.pointer = broker;
		this->bavec.push_back(bapair);
	}
	else{
		broker = bavec[brokerindex].pointer;
		return 0;
	}

cout << "recoverymanager: addBroker " << ip << ":" << port;
	Object::Vector objvec;
	for(int j = 0; j != NUMBER_OF_OBJ_TYPES; j++){
		enum ObjectType t = (enum ObjectType)j;
		objvec.clear();
		this->sm->getObjects(objvec, objectTypeToString(t), broker);
cout << " " << objvec.size();
		for(unsigned i = 0; i != objvec.size(); i++){
			ObjectInfo *objinfo;
			objinfo = newObjectInfoByType(&(objvec[i]), broker, t);
			if(objinfo != NULL)
				this->addObjectInfo(objinfo, t);
		}
	}
cout << "\n";
	logTime("brokerAdded");
	return 0;
}

int RecoveryManager::deleteBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);

	int i = this->getBrokerIndexByAddress(ba);
	if(i == -1)
		return -1;
	//this->sm->delBroker(this->bavec[i].pointer);
	this->bavec.erase(this->bavec.begin() + i);
	return 0;
}

static int getIndexByUrlObjectId(vector<ObjectInfo*> &objs, string url, ObjectId &oid){
	for(unsigned i = 0; i != objs.size(); i++){
		if(oid == objs[i]->getId() && url.compare(objs[i]->getBrokerUrl()) == 0)
			return (signed)i;
	}
	return -1;
}

static void copyBridges(vector<ObjectInfo*> &links, vector<ObjectInfo*> &bridges,
Object::Vector &linkobjlist,
string oldsrc, string newsrc,
bool changedst = false, string olddst = "", string newdst = ""){
	int copycount=0;
	cout << bridges.size() << " bridges in total\n";
	for(vector<ObjectInfo*>::iterator i = bridges.begin(); i != bridges.end(); i++){
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
	vector<ObjectInfo*> exchanges, queues, bindings, brokers, links, bridges;
	Listener *listener;
	char failip[32];
	unsigned failport;
	char backupip[32];
	unsigned backupport;

private:
	void cpVec(vector<ObjectInfo*> &from, vector<ObjectInfo*> &to){
		to.clear();
		for(unsigned i = 0; i != from.size(); i++){
			to.push_back(from[i]);
		}
	}

public:
	CopyObjectsArgs(
	vector<ObjectInfo*> &cpexchanges,
	vector<ObjectInfo*> &cpqueues,
	vector<ObjectInfo*> &cpbindings,
	vector<ObjectInfo*> &cpbrokers,
	vector<ObjectInfo*> &cplinks,
	vector<ObjectInfo*> &cpbridges,
	Listener *cplistener,
 	const char *cpfailip, unsigned cpfailport,
	const char *cpbackupip, unsigned cpbackupport
	){
		cpVec(cpexchanges, exchanges);
		cpVec(cpqueues, queues);
		cpVec(cpbindings, bindings);
		cpVec(cpbrokers, brokers);
		cpVec(cplinks, links);
		cpVec(cpbridges, bridges);
		this->listener = cplistener;
		strcpy(failip, cpfailip);
		failport = cpfailport;
		strcpy(backupip, cpbackupip);
		backupport = cpbackupport;
	}
};

static void *copyObjectThread(void *voidcoarg){
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
	SessionManager backupsm; // = *(this->sm);
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
		vector<ObjectInfo*> *vecs[4] =
		{&(coarg->exchanges), &(coarg->queues), &(coarg->bindings), &(coarg->links)};

		for(unsigned i = 0; i < 4; i++){
			vector<ObjectInfo*> &v = *(vecs[i]);
			copycount=0;
			cout << v.size() <<" objects(" <<i << ") in total\n";
			for(vector<ObjectInfo*>::iterator j = v.begin(); j != v.end(); j++){
				if(oldurl.compare((*j)->getBrokerUrl()) != 0)
					continue;
				if(i == 2){// bindings
					if(((BindingInfo*)(*j))->setBindingNames(
					coarg->exchanges, coarg->queues) == 0)
						continue;
				}
				if(i == 3){
					string linkdest=((LinkInfo*)(*j))->getLinkDestUrl();
					char dstip[64];
					unsigned dstport;
					urlToIPPort(linkdest, dstip, dstport);
					if(checkFailure(dstip) == true){
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

	//assume backupsm has received link objects

	cout.flush();

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
	pthread_create(&threadid, NULL, copyObjectThread, coarg);
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
	vector<ObjectInfo*>::iterator linkiter;

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
		int i = this->getBrokerIndexByAddress(ba);
		if(i == -1)
			return -1;
		Broker* srcbroker = this->bavec[i].pointer;
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
	ListenerEvent *le = ListenerEvent::receivePtr(this->eventpipe[0]);

	switch(le->getType()){
	case OBJECTPROPERTY:
		/*{
			ObjectPropertyEvent *ope = (ObjectPropertyEvent*)le;
			this->addObjectInfo(ope->getObjectInfo(), ope->objtype);
		}*/
		break;
	case BROKERDISCONNECTION:
		break;
	case LINKDOWN:
		break;
	}
	return le;
}

string RecoveryManager::getLinkDst(int index){
	return ((LinkInfo*)(this->links[index]))->getLinkDestUrl();
}

int RecoveryManager::firstLinkInfo(string srcurl){
	return nextLinkInfo(srcurl, -1);
}

int RecoveryManager::nextLinkInfo(string srcurl, int index){
	index++;
	while(((unsigned)index) < this->links.size()){
		if(srcurl.compare(links[index]->getBrokerUrl()) == 0){
			return index;
		}
		index++;
	}
	return -1;
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
	//altbavec.clear();
}

RecoveryManager::~RecoveryManager(){
	delete sm;
	delete listener;
	close(eventpipe[0]);
	close(eventpipe[1]);
}

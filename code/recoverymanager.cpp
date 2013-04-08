#include"recoverymanager.h"
#include<unistd.h>
#include<iostream>
#include"timestamp.h"
#include"messagecopy.h"


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
	for(unsigned j=0; pattern[j] != NULL; j++){
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
			cout << str << " is ignored\n";
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
			delete ((*vec)[i]);
			vec->erase(vec->begin() + i);
			break;
		}
	}
	vec->push_back(objinfo);
	return 0;
}

int RecoveryManager::addBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);

	if(this->getBrokerIndexByAddress(ba) >= 0)
		return -1;

	qpid::client::ConnectionSettings settings;
	settings.host = ba.ip;
	settings.port = ba.port;
	Broker* broker;
	broker = this->sm->addBroker(settings);
	broker->waitForStable();
	Object::Vector objvec;
	for(int j = 0; j != NUMBER_OF_OBJ_TYPES; j++){
		enum ObjectType t = (enum ObjectType)j;
		objvec.clear();
		this->sm->getObjects(objvec, objectTypeToString(t), broker);
		for(unsigned i = 0; i != objvec.size(); i++){
			ObjectInfo *objinfo;
			objinfo = newObjectInfoByType(&(objvec[i]), broker, t);
			if(objinfo != NULL)
				this->addObjectInfo(objinfo, t);
		}
	}
	{
		struct BrokerAddressPair bapair;
		bapair.address = ba;
		bapair.pointer = broker;
		this->bavec.push_back(bapair);
	}
	return 0;
}

int RecoveryManager::deleteBroker(const char *ip, unsigned port){
	BrokerAddress ba((string)ip, port);

	int i = this->getBrokerIndexByAddress(ba);
	if(i == -1)
		return -1;
	this->sm->delBroker(this->bavec[i].pointer);
	this->bavec.erase(this->bavec.begin() + i);
	return 0;
}

static int getIndexByObjectId(vector<ObjectInfo*> &objs, ObjectId &oid){
	for(unsigned i = 0; i != objs.size(); i++){
		if(oid == objs[i]->getId())
			return (signed)i;
	}
	return -1;
}

void RecoveryManager::copyBridges(Object::Vector &linkobjlist,
string oldsrc, string newsrc, bool changedst, string olddst, string newdst){
	int copycount=0;
	cout << this->bridges.size() << " bridges in total\n";
	for(vector<ObjectInfo*>::iterator i = this->bridges.begin(); i != this->bridges.end(); i++){
		string src = (*i)->getBrokerUrl();
		if(oldsrc.compare(src) != 0)
			continue;

		string dst;
		{// find dst
			ObjectId oldlinkid = ((BridgeInfo*)(*i))->getLinkId();
			int j = getIndexByObjectId(this->links, oldlinkid);
			if(j < 0)
				continue;
			if(src.compare(this->links[j]->getBrokerUrl()) != 0){
				cerr << "link id & broker url not match\n";
				continue;
			}
			dst = ((LinkInfo*)(this->links[j]))->getLinkDestUrl();
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

int RecoveryManager::copyObjects(const char *failip, unsigned failport,
const char *backupip, unsigned backupport){
	BrokerAddress oldba(failip, failport);
	BrokerAddress newba(backupip, backupport);
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
		return -1;
	}
	Object &backupbrokerobj = (backupbrokerobjlist[0]);

	{
		vector<ObjectInfo*> *vecs[4] =
		{&(this->exchanges), &(this->queues), &(this->bindings), &(this->links)};

		for(unsigned i = 0; i < 4; i++){
			vector<ObjectInfo*> &v = *(vecs[i]);
			copycount=0;
			cout << v.size() <<" objects(" <<i << ") in total\n";
			for(vector<ObjectInfo*>::iterator j = v.begin(); j != v.end(); j++){
				if(oldurl.compare((*j)->getBrokerUrl()) != 0)
					continue;
				if(i == 2){// bindings
					if(((BindingInfo*)(*j))->setBindingNames(
					this->exchanges, this->queues) == 0)
						continue;
				}
				(*j)->copyTo(&backupbrokerobj);
				copycount++;
			}
			cout << copycount << " objects copied\n";
		}
	}

	//assume backupsm has received link objects

	Object::Vector objlist;
	backupsm.getObjects(objlist, "link", backupbroker);
	this->copyBridges(objlist, oldurl, newurl, false);

	//client messages
	for(unsigned i = 0; i < objlist.size()/* && i < 1*/; i++){
		MessageCopier msgcopy(newurl, linkObjectDest(objlist[i]));
		for(vector<ObjectInfo*>::iterator j = this->queues.begin(); j != this->queues.end(); j++){
			if(oldurl.compare((*j)->getBrokerUrl()) != 0)
				continue;
			msgcopy.copyQueueMessage(((QueueInfo*)(*j))->getName());
		}
	}
	return 0;
}

int RecoveryManager::reroute(const char *srcip, unsigned srcport,
const char *oldip, unsigned oldport, const char *newip, unsigned newport){
	string srcurl = IPPortToUrl(srcip, srcport);
	string newurl = IPPortToUrl(newip, newport);
	string oldurl = IPPortToUrl(oldip, oldport);
	// find link from src to old
	vector<ObjectInfo*>::iterator linkiter;

	for(linkiter = this->links.begin(); linkiter != this->links.end(); linkiter++){
		string linksrc = ((LinkInfo*)(*linkiter))->getBrokerUrl();
		string linkdst = ((LinkInfo*)(*linkiter))->getLinkDestUrl();
		if(srcurl.compare(linksrc) == 0 && oldurl.compare(linkdst) == 0)
			break;
	}
	if(linkiter == this->links.end()){
		cerr << "reroute error: old link not found\n";
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

	this->copyBridges(linkobjlist, srcurl, srcurl, true, oldurl, newurl);

	return 0;
}


int RecoveryManager::getEventFD(){
	return this->eventpipe[0];
}

ListenerEvent *RecoveryManager::handleEvent(){
	ListenerEvent *le = ListenerEvent::receivePtr(this->eventpipe[0]);

	switch(le->getType()){
	case OBJECTPROPERTY:
		{
			ObjectPropertyEvent *ope = (ObjectPropertyEvent*)le;
			this->addObjectInfo(ope->getObjectInfo(), ope->objtype);
		}
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


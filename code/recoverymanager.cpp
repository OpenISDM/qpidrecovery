#include"recoverymanager.h"
#include<unistd.h>
#include<iostream>
#include"timestamp.h"

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

int RecoveryManager::copyObjects(const char *failip, unsigned failport,
const char *backupip, unsigned backupport){
	BrokerAddress oldba(failip, failport);
	BrokerAddress newba(backupip,backupport);
	cout << "failure: " << oldba.getAddress() << "\n";
	cout << "replace: " << newba.getAddress() << "\n";
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
/*
	for(unsigned i=0; i != links.size(); i++){ // link to failed broker
		string dst = ((LinkInfo*)(links[i]))->getLinkDestUrl();
		if(old.getAddress().compare(dst) != 0){
			continue;
		}
		// objptr = 
		((LinkInfo*)(links[i]))->copyToNewDest(objptr,
		(string)(newba.ip), newba.port);
		copycount++;
	}
*/
	{
		vector<ObjectInfo*> *vecs[4] =
		{&(this->exchanges), &(this->queues), &(this->bindings), &(this->links)};

		for(unsigned i = 0; i < 4; i++){
			vector<ObjectInfo*> &v = *(vecs[i]);
			copycount=0;
			cout << v.size() <<" objects(" <<i << ") in total\n";
			for(vector<ObjectInfo*>::iterator j = v.begin(); j != v.end(); j++){
				if(oldba.getAddress().compare((*j)->getBrokerUrl()) != 0)
					continue;
				if(i == 2){//bindings
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
	copycount=0;
	Object::Vector objlist;
	backupsm.getObjects(objlist, "link", backupbroker);
	cout << this->bridges.size() << " bridges in total\n";
	for(vector<ObjectInfo*>::iterator i = this->bridges.begin(); i != this->bridges.end(); i++){
		string src = (*i)->getBrokerUrl(), dst;
		if(oldba.getAddress().compare(src) != 0)
			continue;

		{// search dst url
			ObjectId oldlinkid = ((BridgeInfo*)(*i))->getLinkId();
			vector<ObjectInfo*>::iterator j;
			for(j = this->links.begin(); j != this->links.end(); j++){// search failed link
				if((*j)->getId() == oldlinkid &&
				(*j)->getBrokerUrl().compare(src) == 0)
					break;
			}
			if(j == this->links.end()){
				cerr << "disconnectHandler: cannot find link by id\n";
				cerr.flush();
				continue;
			}
			dst = ((LinkInfo*)(*j))->getLinkDestUrl();
		}
		cout << src << "->" << dst << "\n";
		for(Object::Vector::iterator j = objlist.begin(); j != objlist.end(); j++){
			if(
			src.compare((j->getBroker())->getUrl()) == 0 &&
			dst.compare(linkObjectDest(*j)) == 0){
				(*i)->copyTo(&(*j));
				break;
			}
		}
		copycount++;
	}
/*
	copycount=0;
	cout << bridges.size() << " bridges in total\n";
	for(unsigned i = 0; i != bridges.size(); i++){
		string dest, src = bridges[i]->getBrokerUrl(), newdest, newsrc;
		ObjectId oldlinkid = ((BridgeInfo*)(bridges[i]))->getLinkId();
		unsigned j;
		for(j = 0; j < links.size(); j++){// search failed link
			if(links[j]->getId() == oldlinkid &&
			links[j]->getBrokerUrl().compare(src) == 0)
				break;
		}
		if(j >= links.size()){
			cerr << "disconnectHandler: cannot find link by id\n";cerr.flush();
			continue;
		}
		dest=((LinkInfo*)(links[j]))->getLinkDestUrl();
		if(fba.getAddress().compare(src)==0){
			newsrc = rba.getAddress();
			newdest = dest;
		}
		else if(fba.getAddress().compare(dest)==0){
			newsrc = src;
			newdest = rba.getAddress();
		}
		else{
			continue;
		}
		cout << src << " ->"<< dest << newsrc << "->" << newdest << "\n";
		{
			int j = getBrokerIndexByString(bavec, newsrc);
if(j==-1){cerr<< "j error bridge\n";cerr.flush();}
			sm.getObjects(objlist, "link", bavec[j].pointer);
			objptr = findLinkInList(objlist, newsrc, newdest);
if(objptr==NULL){cerr<<"linkobj error\n";cerr.flush();}
		}
		if(objptr == NULL)
			continue;
		bridges[i]->copyTo(objptr);
		copycount++;
	}
*/
	cout << copycount << " bridges copied\n";
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
	//altbavec.clear();
}

RecoveryManager::~RecoveryManager(){
	delete sm;
	delete listener;
	close(eventpipe[0]);
	close(eventpipe[1]);
}


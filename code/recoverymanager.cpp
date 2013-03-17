#include"recoverymanager.h"
#include<iostream>

using namespace std;

int RecoveryManager::getBrokerIndex(BrokerAddress& ba){
	for(unsigned i=0; i < this->bavec.size(); i++){
		if(this->bavec[i].address == ba)
			return (int)i;
	}
	return -1;
}

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

	if(this->getBrokerIndex(ba) >= 0)
		return -1;

	qpid::client::ConnectionSettings settings;
	settings.host = ba.ip;
	settings.port = ba.port;
	Broker* broker;
	broker = this->sm->addBroker(settings);
	broker->waitForStable();
	//any other method to get objects from broker immediately?
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

RecoveryManager::RecoveryManager(){
	listener = new Listener();
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
}


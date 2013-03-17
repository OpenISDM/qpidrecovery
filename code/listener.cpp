#include"listener.h"
#include"brokerobject.h"
#include<unistd.h>
#include<memory.h>

using namespace std;

double getSecond();

//class Listener

int Listener::writeEvent(struct ListenerEvent& le){
	return write(pipefd[1], &le, sizeof(ListenerEvent));
}

Listener::Listener(){
	if(pipe(this->pipefd) == -1)
		cerr << "cannot create pipe\n";
	fs = new FileSelector(default_timeout, 0);
	registerFD(this->pipefd[0]);
}

Listener::~Listener(){
	delete fs;
	close(pipefd[0]);
	close(pipefd[1]);
}

void Listener::brokerConnected(const Broker& broker) {
	cout << "brokerConnected: " << broker << endl;
}
void Listener::brokerDisconnected(const Broker& broker) {
	struct ListenerEvent le;
	le.type = BROKERDISCONNECT;
	string url=broker.getUrl();
	cout << "brokerDisconnected: " << url << endl;
	for(unsigned i=0; i<url.length(); i++)
		le.connecturl[i]=url[i];
	le.connecturl[url.length()]='\0';
	le.bptr=(Broker*)&broker;
	cout << "passing event: return " << writeEvent(le) << endl;
}
void Listener::newPackage(const std::string& name) {
	cout << "newPackage: " << name << endl;
	cout << getSecond() <<endl;
}
void Listener::newClass(const ClassKey& classKey) {
	cout << "newClass: key=" << classKey << endl;
	cout << getSecond() <<endl;
}
void Listener::newAgent(const Agent& agent) {
	cout << "newAgent: " << agent << endl;
	cout << getSecond() <<endl;
}
void Listener::delAgent(const Agent& agent) {
	cout << "delAgent: " << agent << endl;
	cout << getSecond() <<endl;
}
void Listener::objectProps(Broker& broker, Object& object) {
	//discover new objects here
	cout << "objectProps: broker=" << broker << " object=" << object << endl;
	cout << getSecond() <<endl;
	objectProps2(broker, object);
}

ObjectInfo*newObjectInfoByType(Object& obj, Broker*broker, enum ObjectType t);
// in brokerobject.cpp

void Listener::objectProps2(Broker& broker, Object& object) {
	//discover new objects here
	//cout << "objectProps: broker=" << broker << " object=" << object << endl;
	string classname = object.getClassKey().getClassName();
	struct ListenerEvent le;
	enum ObjectType t=objectStringToType(classname);

	le.type=NEWOBJECT;
	le.objtype=t;
	le.objinfo=newObjectInfoByType(&object, &broker, t);
	if(le.objinfo != NULL)
		writeEvent(le);
}

void Listener::objectStats(Broker& broker, Object& object) {
    cout << "objectStats: broker=" << broker << " object=" << object << endl;
    cout << getSecond() <<endl;
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
}
void Listener::event(Event& event) {/*
    const string bind = "bind";
    const struct SchemaClass* sc = event.getSchema();
    string classname = sc->key.getClassName();
    cout << "event: " << event << endl;

    if(classname.compare(bind) == 0){
        cout << "event: " << classname << endl <<
        "    exchange: " << event.attrString("exName") << endl <<
        "    queue: " << event.attrString("qName") << endl;
    }*/
    cout << event << endl;
    cout << getSecond() <<endl;
}

void Listener::registerFD(int fd){
	fs->registerFD(fd);
}

void Listener::unregisterFD(int fd){
	fs->unregisterFD(fd);
}

int Listener::readEvent(struct ListenerEvent& le){
	int r = -1;
	int f = fs->getReadReadyFD();
	switch(f){
	case -2:
		fs->resetTimeout(default_timeout, 0);
		le.type = TIMEOUT;
		r = 0;
		break;
	case -1:
		cerr << "select error";
		break;
	default:
		if(f == pipefd[0]){
			r = read(f, &le, sizeof(struct ListenerEvent));
		}
		else{
			le.type=FILEREADY;
			le.readyfd = f;
			r = 0;
		}
		break;
	}
	return r;
}


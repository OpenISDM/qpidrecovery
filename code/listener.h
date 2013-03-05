#ifndef LISTENER_H_DEFINED
#define LISTENER_H_DEFINED

#include "qpid/console/ConsoleListener.h"
#include "qpid/console/SessionManager.h"
#include "brokerobject.h"
#include "fileselector.h"

using namespace qpid::console;

enum ListenerEventType{
	BROKERDISCONNECT,
	BYTEDEQUEUE,
	BYTEENQUEUE,
	TIMEOUT,
	NEWOBJECT,
	FILEREADY
};
struct ListenerEvent{
	enum ListenerEventType type;
	Broker*bptr;
	char connecturl[32];
	ObjectId oid;
	unsigned long long ullvalue;// need better design
	unsigned uvalue;
	enum ObjectType objtype;
	ObjectInfo* objinfo;
	int readyfd;
};

class Listener : public ConsoleListener {
private:
	int pipefd[2];
	FileSelector *fs;

	int writeEvent(struct ListenerEvent& le);
public:
	static const long default_timeout = 3;
	Listener();
	~Listener();

	void brokerConnected(const Broker& broker);
	void brokerDisconnected(const Broker& broker);
	void newPackage(const std::string& name);
	void newClass(const ClassKey& classKey);
	void newAgent(const Agent& agent);
	void delAgent(const Agent& agent);
	void objectProps2(Broker& broker, Object& object);
	void objectProps(Broker& broker, Object& object);
	void objectStats(Broker& broker, Object& object);
	void event(Event& event);

	void registerFD(int fd);
	void unregisterFD(int fd);
	int readEvent(struct ListenerEvent& le);
};

#endif

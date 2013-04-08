#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Session.h>

using namespace qpid::messaging;

class MessageCopier{
private:
	Connection *srcconn, *dstconn;
	Session srcsess, dstsess;
public:
	MessageCopier(std::string srcbroker, std::string dstbroker);
	~MessageCopier();

	int copyQueueMessage(std::string queuename);
};


/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Message.h>
#include <vector>

using namespace qpid::messaging;

typedef std::vector<Message> MessageVector;

class QpidClient{
private:
	Connection *conn;
	Session sess;
	Connection *conn2;
	Session sess2;
public:
	QpidClient(std::string brokerurl, std::string targeturl = "");
	~QpidClient();
	int sendRandomString(std::string targetname);
	int send(std::string targetname, MessageVector &msgvec);
	int receive(std::string queuename, MessageVector &msgvec);
	int receiveSend(std::string sourcename, std::string targetname);
};


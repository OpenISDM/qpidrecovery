#include"common.h"
#define HEARTBEAT_PERIOD (1)

using namespace std;

class HeartbeatClient{
private:
	char ip[IP_LENGTH];
	int sfd;
	unsigned timestamp;
public:
	HeartbeatClient(const char *ip);
	~HeartbeatClient();
	int readMessage(); // return number of bytes read, 0 or -1 if tcp shutdown
	int isExpired(unsigned t);
	int getFD();
	std::string getIP();
	void getIP(char *s);
};

#ifndef BROKERADDRESS_H_DEFINED
#define BROKERADDRESS_H_DEFINED

#include<string>
using namespace std;

class BrokerAddress{
public:
	char ip[20];
	unsigned port;

	BrokerAddress();
	BrokerAddress(string ip, unsigned port);
	string getAddress();
	bool operator==(BrokerAddress ba);
	BrokerAddress operator=(BrokerAddress ba);
};

#endif

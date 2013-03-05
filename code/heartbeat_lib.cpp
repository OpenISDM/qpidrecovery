#include<iostream>
#include"heartbeat_lib.h"
#include"socketlib.h"
#include<cstring>
#include<ctime>

using namespace std;

HeartbeatClient::HeartbeatClient(const char *ip){
	strcpy(this->ip, ip);
	sfd = tcpconnect(ip, HEARTBEAT_PORT);
	this->timestamp = (unsigned)time(NULL);
}

HeartbeatClient::~HeartbeatClient(){
	close(sfd);
}

int HeartbeatClient::readMessage(){
	struct Heartbeat hb;
	int r;
	r = read(sfd, &hb, sizeof(struct Heartbeat));
	if(r == sizeof(struct Heartbeat)){
		this->timestamp = (unsigned)time(NULL);
		cout << hb.name << "\n";
	}
	return r;
}

int HeartbeatClient::isExpired(unsigned t){
	return (((unsigned)time(NULL)) - this->timestamp) > t;
}

int HeartbeatClient::getFD(){
	return this->sfd;
}
string HeartbeatClient::getIP(){
	return (string)(this->ip);
}
void HeartbeatClient::getIP(char* s){
	strcpy(s, this->ip);
}

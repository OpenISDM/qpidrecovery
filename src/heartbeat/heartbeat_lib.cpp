/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include<iostream>
#include"heartbeat_lib.h"
#include"socketlib.h"
#include"timestamp.h"
#include<cstring>

using namespace std;

HeartbeatClient::HeartbeatClient(const char *ip){
	strcpy(this->ip, ip);
	sfd = tcpconnect(ip, HEARTBEAT_PORT);
	this->timestamp = getSecond();
}

HeartbeatClient::~HeartbeatClient(){
	if(sfd >= 0)
		close(sfd);
}

int HeartbeatClient::readMessage(){
	struct Heartbeat hb;
	int r;
	if(sfd < 0)
		return -1;
	r = read(sfd, &hb, sizeof(struct Heartbeat));
	if(r == sizeof(struct Heartbeat)){
		this->timestamp = getSecond();
		// cout << hb.name << "\n";
	}
	else r = -1;
	return r;
}

int HeartbeatClient::isExpired(double t){
	return (getSecond() - this->timestamp) > t;
}

int HeartbeatClient::getFD(){
	return this->sfd;
}
string HeartbeatClient::getIP(){
	return (string)(this->ip);
}
void HeartbeatClient::getIP(char *s){
	strcpy(s, this->ip);
}


#include<cstdio>
#include<cstring>
#include<iostream>
#include"brokeraddress.h"

//class BrokerAddress
BrokerAddress::BrokerAddress(){
}

BrokerAddress::BrokerAddress(string ip, unsigned port){
	strcpy(this->ip,ip.c_str());
	this->port = port;
}

static string unsignedToString(unsigned x){
	char s[12];
	sprintf(s, "%u", x);
	return (string)s;
}

string BrokerAddress::getAddress(){
	return ((string)this->ip) + ":" + unsignedToString(this->port);
}

bool BrokerAddress::operator==(BrokerAddress ba){
	return this->port == ba.port && strcmp(this->ip,ba.ip) == 0;
}

BrokerAddress BrokerAddress::operator=(BrokerAddress ba){
	strcpy(this->ip, ba.ip);
	this->port = ba.port;
	return *this;
}


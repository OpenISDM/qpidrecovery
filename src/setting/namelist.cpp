#include<cstring>
#include<string>
#include<iostream>
#include<vector>
#include<cstdio>
#include<cstdlib>
#include<unistd.h>
#include"timestamp.h"
#include"socketlib.h"
#include"common.h"
#include"namelist.h"


bool checkFailure(const char *checkip){

	int sfd = tcpconnect(checkip, HEARTBEAT_PORT);
	if(sfd < 0)
		return true;
	else{
		close(sfd);
		return false;
	}
}

#define URL_MAX_LENGTH (64)
const char urlformat[] = "%63s"; // 63 characters + '\0'
struct UrlString{
	char str[URL_MAX_LENGTH];
};

std::vector<struct UrlString> monitoredbrokers, backupbrokers;

static int readBrokerArgument(int argc, char *argv[], std::vector<struct UrlString> &brokers){
	struct UrlString u;
	FILE *f = fopen(argv[0], "r");
	if(f == NULL){
		std::cerr << "cannot open " << argv[0] << std::endl;
		exit(0);
	}

	brokers.clear();
	while(fscanf(f, urlformat, u.str) == 1){
			brokers.push_back(u);
	}
	fclose(f);
	return 0;
}

int readBackupBrokerArgument(int argc, char *argv[]){ // TODO: add score
	return readBrokerArgument(argc, argv, backupbrokers);
}

int readMonitoredBrokerArgument(int argc, char *argv[]){
	return readBrokerArgument(argc, argv, monitoredbrokers);
}

const char *getSubnetBroker(){
	static unsigned moniindex = 0;
	// we do not allow add or delete after reading argument
	if(moniindex >= monitoredbrokers.size()){
		return monitoredbrokers[moniindex].str;
	}
	return NULL;
}

const char *getBackupBroker(const char*failip, unsigned &port, int &score){
	static unsigned backindex = 0;
	// we do not allow add or delete after reading argument
	if(backindex >= backupbrokers.size()){
		return backupbrokers[backindex].str;
	}
	return NULL;
}

const char *getAcceptor(const char *servicename){
	std::cerr << "TODO: distributed mode\n";
	return NULL;
}
const char *getProposer(const char *servicename){
	std::cerr << "TODO: distributed mode\n";
	return NULL;
}

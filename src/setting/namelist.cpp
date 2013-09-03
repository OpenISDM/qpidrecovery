/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

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

template<typename T>
class VectorAndIndex :public std::vector<T>{
public:
	unsigned index;

	VectorAndIndex(){
		this->clear();
		this->index = 0;
	}

	T *getAnElement(){
		// we do not allow add or delete after reading argument
		if(index < this->size()){
			index++;
			return &((*this)[index - 1]);
		}
		return NULL;
	}
};

#define URL_MAX_LENGTH (64)
const char urlformat[] = "%63s"; // 63 characters + '\0'
struct UrlString{
	char str[URL_MAX_LENGTH];
};

struct VectorAndIndex<struct UrlString> monitoredbrokers, backupbrokers, acceptors, proposers;

static int readNameList(const char *filename, VectorAndIndex<struct UrlString> &vec){
	struct UrlString u;
	FILE *f = fopen(filename, "r");
	if(f == NULL){
		std::cerr << "cannot open " << filename << std::endl;
		exit(0);
	}

	while(fscanf(f, urlformat, u.str) == 1){
			vec.push_back(u);
	}
	fclose(f);
	return 0;
}

int readBackupBrokerArgument(int argc, const char *argv[]){
	return readNameList(argv[0], backupbrokers);
}

int readMonitoredBrokerArgument(int argc, const char *argv[]){
	return readNameList(argv[0], monitoredbrokers);
}

int readProposerArgument(int argc, const char *argv[]){
	return readNameList(argv[0], proposers);
}

int readAcceptorArgument(int argc, const char *argv[]){
	return readNameList(argv[0], acceptors);
}

const char *getSubnetBrokerURL(){
	return monitoredbrokers.getAnElement()->str;
}

const char *getBackupBrokerURL(const char*failip, unsigned failport, int &score){
	// TODO: determine the score and select backup by failip, port
	score = 100;
	return backupbrokers.getAnElement()->str;
}

const char *getAcceptorIP(){
	return acceptors.getAnElement()->str;
}
const char *getProposerIP(){
	return proposers.getAnElement()->str;
	
}

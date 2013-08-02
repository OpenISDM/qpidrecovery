#include<cstring>
#include<string>
#include<iostream>
#include<cstdio>
#include<cstdlib>
#include<unistd.h>
#include"timestamp.h"
#include"socketlib.h"
#include"common.h"
#include"discovery.h"

const bool separate_service = false;
const char *fixed_backup_ip = "1.0.5.1";

bool isCentralizedMode(){
	return true;
}

static const char *hostNameToIP(const char *name){
struct IpName{
	char ip[32];
	char hostname[32];
};
static struct IpName ipname[150];

	static bool firsttime = true;
	if(firsttime){
		firsttime = false;
		FILE *filein = fopen("/home/estinet/ip2service.txt", "r");
		unsigned i;
		for(i = 0; fscanf(filein, "%s %s", ipname[i].hostname, ipname[i].ip) == 2; i++);
		ipname[i].ip[0] = ipname[i].hostname[0] = '\0';
		fclose(filein);
	}

	for(int i = 0; ipname[i].ip[0] != '\0' ; i++){
		if(strcmp(ipname[i].hostname, name) == 0)
			return ipname[i].ip;
	}
	std::cerr << "discovery: cannot map hostname " << name << " to ip\n";
	return NULL;
}

static int getIPNumber(const char *ip){
	for(int a = 0; a < 2; a++){
		while(*ip != '.')
			ip++;
		ip++;
	}
	int r;
	for(r = 0; *ip >= '0' && *ip <= '9'; ip++)
		r = r * 10 + *ip - '0';
	return r;
}

static int getHostNumber(const char *servicename){
	while((*servicename < '0' || *servicename > '9') && *servicename != '\0'){
		servicename++;
	}
	int r;
	for(r = 0; *servicename >= '0' && *servicename <= '9'; servicename++){
		r = r * 10 + (*servicename - '0');
	}
	return r;
}

bool checkFailure(const char *checkip){
	static char faillist[32][32];
	static int failcount = -1;
/*
{
	int sfd = tcpconnect(checkip, HEARTBEAT_PORT);
	if(sfd < 0)
		return true;
	else{
		close(sfd);
		return false;
	}
}
*/
	if(failcount == -1){
		FILE *f = fopen("/home/estinet/flist.txt", "r");
		for(failcount = 0; fscanf(f, "%s", faillist[failcount]) == 1; failcount++){
		}
		fclose(f);
	}
	if(getSecond() < FAIL_TIME)
		return false;
	for(int c = 0; c < failcount; c++){
		if(strcmp(checkip, hostNameToIP(faillist[c])) == 0)
			return true;
	}
	return false;
}

// this class is only for NBROKER subnet
class NearServiceList{
private:
	char nearhost[NBROKER][32];
	int count;


public:
	NearServiceList(const char *servicename){
		int hostnumber = getHostNumber(servicename) - 1;
		for(int a = 1; a <= NBROKER; a++){
			int b = (hostnumber + (a % 2 == 0? a / 2: NBROKER - a / 2)) % NBROKER;
			sprintf(nearhost[a - 1], "host%d", 1 + b);
		}
		this->count = 0;
	}
	const char *nextServiceName(){
		while(1){
			this->count = (this->count + 1) % NBROKER;
			if(checkFailure(hostNameToIP(this->nearhost[this->count])) == false)
				break;
			//usleep(100*1000);
		}
		return (const char*)nearhost[count];
	}
};


bool isActiveInExperiment(const char *servicename){
	const int activelist[]={
		 1, 2, 3, 4, 5, 6,
		 7, 8, 9,10,11,12,
		13,14,15,16,17,18,
		19,20,21,22,23,24,
		25,26,27,28,29,30,
		31,32,33,34,35,36,
		37,38,39,40,41,42,
		43,44,45,46,47,48,
		49,50,
		-1
	};
	for(int i = 0; activelist[i] >= 0; i++){
		char s[32];
		if(activelist[i] > NBROKER)
			continue;
		sprintf(s,"host%d", activelist[i]);
		if(strcmp(s, servicename) == 0)
			return true;
	}
	return false;
}

static bool isBrokerService(const char *servicename){
	if(separate_service == false)
		return true;
	return (getHostNumber(servicename) % 2 == 0);
}

static bool isRecoveryService(const char *servicename){
	if(separate_service == false)
		return true;
	return (! isBrokerService(servicename));
}

const char *discoverAcceptor(const char *servicename){
	if(isCentralizedMode()){
		return hostNameToIP("host1");
	}
	// DISTRIBUTED_MODE
	static NearServiceList *nslist = NULL;
	if(nslist == NULL)
		nslist = new NearServiceList(servicename);
	while(1){
		const char *acceptorname = nslist->nextServiceName();
		if(isRecoveryService(acceptorname) == true){
			return hostNameToIP(acceptorname);
		}
	}
}
const char *discoverProposer(const char *servicename){
	if(isCentralizedMode()){
		return hostNameToIP("host1");
	}
	// DISTRIBUTED_MODE
	static NearServiceList *nslist = NULL;
	if(nslist == NULL)
		nslist = new NearServiceList(servicename);
	while(1){
		const char *proposername = nslist->nextServiceName();
		if(isRecoveryService(proposername) == true){
			return hostNameToIP(proposername);
		}
	}
}

const char *discoverBackup(const char *localip, const char*failip,
unsigned &port, int &score){
	static int backupcount = 0;
	int fnumber = getIPNumber(failip);
	char failservicename[64];
	sprintf(failservicename, "host%d", fnumber);

	NearServiceList backuplist(failservicename);
	while(1){
		const char *bip;
		if(fixed_backup_ip != NULL){
			bip = fixed_backup_ip;
		}
		else{
			const char* bname = backuplist.nextServiceName();
			if(isBrokerService(bname) == false){
				continue;
			}
			bip = hostNameToIP(bname);
			if(strcmp(localip, bip) == 0){
				continue;
			}
		}
		score = getIPNumber(bip) + rand() % 50;
		port = 5672 - 1 + getIPNumber(bip);
		backupcount++;
		return bip;
	}
}


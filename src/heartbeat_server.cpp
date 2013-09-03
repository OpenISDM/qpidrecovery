/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include<iostream>
#include"heartbeat_lib.h"
#include"socketlib.h"
#include"fileselector.h"
#include<vector>
#include<unistd.h>
#include"string.h"

using namespace std;

void sendHeartbeat(vector<int> &sfdvec, const char *servicename){
	struct Heartbeat hb;
	strcpy(hb.name, servicename);
	for(int i = 0; i < (signed int)sfdvec.size(); i++){
		if(write(sfdvec[i], &hb, sizeof(struct Heartbeat)) >= 0){
			continue;
		}
		cerr << "error: write client(" << sfdvec[i] << ")\n";
		close(sfdvec[i]);
		sfdvec.erase(sfdvec.begin() + i);
		i--;
	}
}

int main(int argc,char *argv[]){
	const char *servicename;
	if(argc < 2)
		servicename = "abcdefg";
	else
		servicename = (const char*)argv[1];
	cout << "heartbeat: " << servicename << endl;

	replaceSIGPIPE();

	int sfd;
	vector<int> sfdvec;
	FileSelector fs(HEARTBEAT_PERIOD, 0);

	sfd = tcpserversocket(HEARTBEAT_PORT);
	if(sfd < 0){
		cerr << "error: server socket\n";
		return -1;
	}
	sfdvec.clear();
	fs.registerFD(sfd);
	while(1){
		int ready = fs.getReadReadyFD();
		if(ready == -1){
			cerr << "error: select\n";
			return -1;
		}
		if(ready == -2){
			sendHeartbeat(sfdvec, servicename);
			fs.resetTimeout(HEARTBEAT_PERIOD, 0);
			continue;
		}
		if(ready == sfd){
			char from[128];
			int newsfd = tcpaccept(sfd, from);
			cout << "heartbeat: new client(" << newsfd << "): " << from << endl;
			sfdvec.push_back(newsfd);
			continue;
		}
		cerr << "unrecognized fd\n";
	}
	return 0;
}


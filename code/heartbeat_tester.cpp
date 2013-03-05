#include<iostream>
#include"heartbeat_lib.h"
#include"fileselector.h"

using namespace std;
int main(int argc,char*argv[]){
	char ip[] = "127.0.0.1";
	HeartbeatClient hbc(ip);
	while(1){
		int r;
		r= hbc.readMessage();
		cout << r << "\n";
		if(r<=0)
			break;
	}
	return 0;
}

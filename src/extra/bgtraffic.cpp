#include<cmath>
#include<cstdio>
#include<cstdlib>
#include"fileselector.h"
#include"socketlib.h"
#include"common.h"
#include"timestamp.h"

#define TRAFFIC_SIZE ((128*1024*8)/10)
#define MESSAGE_SIZE (8100)
const double arrivalrate = TRAFFIC_SIZE / (double)MESSAGE_SIZE;

char msg[MESSAGE_SIZE + 4];

double exponentialRandom(double lambda){
	double uniform = ((rand() % 1024) + 1) / 1025.0;
	return log(1 - uniform) / (-lambda);
}

void getInterarrivalTime(unsigned &sec,unsigned &usec){
	double i = exponentialRandom(arrivalrate);
	if(i > 200)
		i = 200;
	sec = (unsigned int)i;
	if(sec > i){
		fprintf(stderr, "bgtraffic: floor error\n");
		sec--;
	}
	usec = (unsigned int)(1000000 * (i - sec));
}

int main(int argc,char *argv[]){

	if(argc < 3){
		fprintf(stderr, "need random seed\n");
		return -1;
	}

	fprintf(stderr, "start bgtraffic %s %s\n", argv[1], argv[2]);
	for(int i = 0; i < MESSAGE_SIZE; i++)
		msg[i] = 'A' + i % 26;

	const char *sendtoip = argv[1];
	unsigned randomseed;
	randomseed = sscanf(argv[2], "%u", &randomseed);
	srand(randomseed);

	int sfd = udpsocket(BGTRAFFIC_PORT);
	if(sfd == -1){
		fprintf(stderr, "udp socket error\n");
		return -1;
	}

	unsigned sec, usec;
	getInterarrivalTime(sec, usec);
	FileSelector fs(sec, usec);

	fs.registerFD(sfd);

	while(1){
		int readyfd = fs.getReadReadyFD();
		if(readyfd == GET_READY_FD_TIMEOUT){
			getInterarrivalTime(sec, usec);
			fs.resetTimeout(sec, usec);
			udpsendto(sfd, sendtoip, BGTRAFFIC_PORT, msg, MESSAGE_SIZE);
			continue;
		}
		if(readyfd == sfd){
			read(sfd, msg, MESSAGE_SIZE);
			continue;
		}
		fprintf(stderr, "select error\n");
		return -1;
	}
	return 0;
}


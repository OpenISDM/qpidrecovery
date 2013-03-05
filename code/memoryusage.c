#include<unistd.h>
#include<fcntl.h>
#include"memoryusage.h"
#include<stdio.h>
/*
more /proc/PID/statm
virtual resident share text library data dirty

unit=page=4096 bytes
*/

static int readstatm(char* buf){
	int fd;
	sprintf(buf,"/proc/%u/statm",getpid());
	fd=open(buf, O_RDONLY);

	if(fd<0){
		fprintf(stderr, "open %s error\n", buf);
		return -1;
	}
	if(read(fd, buf, 256)<=0){
		fprintf(stderr, "read error\n");
		return -1;
	}
	close(fd);
	return 0;
}

int getMemoryUsage(unsigned* virtmem,unsigned* resimem){
	char buf[256];
	const unsigned pagesize=4096;
	if(readstatm(buf)<0)
		return -1;
	sscanf(buf, "%u%u",virtmem, resimem);
	(*virtmem)*=pagesize;
	(*resimem)*=pagesize;
	return 0;
}


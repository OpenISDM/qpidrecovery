/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include"fileselector.h"
#include<sys/select.h>
#include<unistd.h>

using namespace std;

// class FileSelector
FileSelector::FileSelector(unsigned s, unsigned us){
	this->numberofreg=0;
	this->resetTimeout(s, us);
}

FileSelector::~FileSelector(){
}

void FileSelector::resetTimeout(unsigned s, unsigned us){
	this->timeout_s = s;
	this->timeout_us = us;
}

int FileSelector::getReadReadyFD(){
	struct timeval tv = {this->timeout_s, this->timeout_us};
	fd_set rset;
	int r, maxfd;

	FD_ZERO(&rset);
	maxfd = 0;
	for(int i=0; i<this->numberofreg; i++){
		FD_SET(this->regfd[i],&rset);
		if(maxfd<this->regfd[i])
			maxfd=this->regfd[i];
	}

	r = select(maxfd + 1, &rset, NULL, NULL, &tv);
	this->timeout_s = tv.tv_sec;
	this->timeout_us = tv.tv_usec;

	if(r == 0) // timeout
		return GET_READY_FD_TIMEOUT;
	if(r < 0) // error
		return -1;
	for(int i = 0; i < this->numberofreg; i++){
		if(FD_ISSET(this->regfd[i], &rset))
			return this->regfd[i];
	}
	return -777; // error?
}

void FileSelector::registerFD(int fd){
	for(int i=0; i<this->numberofreg; i++){
		if(fd==this->regfd[i])
			return;
	}
	this->regfd[this->numberofreg]=fd;
	this->numberofreg++;
}

void FileSelector::unregisterFD(int fd){
	for(int i=0; i<this->numberofreg; i++){
		if(fd==this->regfd[i]){
			this->numberofreg--;
			this->regfd[i]=this->regfd[this->numberofreg];
			return;
		}
	}
}



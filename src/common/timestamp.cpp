#include<sys/time.h>
#include<iostream>
#include"timestamp.h"
#include"cstdio"

double getSecond(){
	struct timeval tv;
	if(gettimeofday(&tv, NULL)!=0)
		std::cerr << "gettimeofday error\n";
	//tv.tv_sec -= subsec;
	return tv.tv_sec+1e-6*tv.tv_usec;
}

static const char *logname = "host??";

void setLogName(const char *name){
	logname = name;
}

void logTime(const char *str){
	static FILE *logf = NULL;

	if(logf == NULL){
		//logf = fopen("/home/estinet/exp_recovery.log", "a");
		logf = fopen("/dev/null", "a");
	}
	fprintf(logf, "%.6lf %s %s\n", getSecond(), logname, str);
	fflush(logf);
}

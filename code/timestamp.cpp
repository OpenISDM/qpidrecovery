#include<sys/time.h>
#include<iostream>

double getSecond(){
	struct timeval tv;
	//const unsigned subsec = 1364241338;
	if(gettimeofday(&tv, NULL)!=0)
		std::cerr << "gettimeofday error\n";
	//tv.tv_sec -= subsec;
	return tv.tv_sec+1e-6*tv.tv_usec;
}


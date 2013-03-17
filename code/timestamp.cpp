#include<sys/time.h>
#include<iostream>

double getSecond(){
	struct timeval tv;
	if(gettimeofday(&tv, NULL)!=0)
		std::cerr << "gettimeofday error\n";
	return tv.tv_sec+1e-6*tv.tv_usec;
}


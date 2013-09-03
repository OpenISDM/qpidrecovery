/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include<iostream>
#include"socketlib.h"
#include"fileselector.h"
#include"common.h"
#include"timestamp.h"
#include"messagecopy.h"
#include<pthread.h>
#include<cstdio>

void *copyThread(void *voidcmr){
	struct CopyMessageRequest *cmr = (struct CopyMessageRequest*)voidcmr;
	char targeturl[128], sourceurl[128];
logTime("startCopyMessage");
	sprintf(targeturl, "%s:%u", cmr->targetip, cmr->targetport);
	sprintf(sourceurl, "%s:%u", cmr->sourceip, cmr->sourceport);
	QpidClient qpidclient((std::string)sourceurl, (std::string)targeturl);
	qpidclient.receiveSend(cmr->sourcename, cmr->targetname);
logTime("finishCopyMessage");
	delete cmr;
	return NULL;
}

int main(int argc,char *argv[]){
	int cmsfd;
	FileSelector fs(1, 0);

	if(argc >= 7){
		struct CopyMessageRequest *p = new struct CopyMessageRequest();
		sscanf(argv[1], "%s", p->targetname);
		sscanf(argv[2], "%s", p->targetip);
		sscanf(argv[3], "%u", &(p->targetport));
		sscanf(argv[4], "%s", p->targetname);
		sscanf(argv[5], "%s", p->sourceip);
		sscanf(argv[6], "%u", &(p->sourceport));
		copyThread((void*)p);
	}
	
	cmsfd = tcpserversocket(MESSAGECOPY_PORT);
	fs.registerFD(cmsfd);

	while(1){
		int ready = fs.getReadReadyFD();
		if(ready == cmsfd){
			char fromip[64];
			struct CopyMessageRequest *p = new struct CopyMessageRequest();
			int sfd = acceptRead(cmsfd, fromip, *p);
			pthread_t tid;
			pthread_create(&tid, NULL, copyThread, p);
			pthread_detach(tid);
			/*
 			void *retptr;
			pthread_join(tid, &retptr);
			*/
			close(sfd);
			continue;
		}
		if(ready == GET_READY_FD_TIMEOUT){
			fs.resetTimeout(1000, 0);
			continue;
		}
		std::cerr << "messagecopy_main: error\n";
	}
}


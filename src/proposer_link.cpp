#include<cstring>
#include"proposer_link.h"
#include"common.h"


LinkDstListener::LinkDstListener(FileSelector &fileselector){
	this->fs = &fileselector;
	this->listening.clear();
}

int LinkDstListener::listenAddressChange(const ReplyProposer &rp, 
const char *sip, unsigned sport,
const char *oip, unsigned oport){
	struct AddressChange ac;
	strcpy(ac.name, rp.name);
	ac.count = rp.length;
	ac.livingcount = 0;

	struct ReplyAddress ra;
	const int rasize = sizeof(struct ReplyAddress);
	strcpy(ra.name, rp.name);
STDCOUT("proposers:");
	for(unsigned i = 0; i != ac.count; i++){
		strcpy(ac.proposers[i], rp.ip[i]);
STDCOUT(" " << ac.proposers[i]);
		ac.sfd[i] = tcpconnect(ac.proposers[i], QUERY_BACKUP_PORT);
		if(ac.sfd[i] >= 0){
			if(write(ac.sfd[i], &ra, rasize) != rasize){
STDCOUT("(write error)");
			}
			this->fs->registerFD(ac.sfd[i]);
			ac.livingcount++;
		}
		else{
			ac.sfd[i] = -1;
STDCOUT("(connect error)");
		}
	}
STDCOUT("\n");
	strcpy(ac.oldip, oip);
	ac.oldport = oport;
	strcpy(ac.srcip, sip);
	ac.srcport = sport;

	if(ac.livingcount == 0)
		return -1;
	this->listening.push_back(ac);
STDCOUT("listen: living count=" << ac.livingcount << "\n");
	return 0;
}

static void closeAll(struct AddressChange &ac, FileSelector &fs){
	for(unsigned i = 0; i != ac.count; i++){
		if(ac.sfd[i] >= 0){
			fs.unregisterFD(ac.sfd[i]);
			close(ac.sfd[i]);
			ac.sfd[i] = -1;
		}
	}
}

int LinkDstListener::readAddressChange(int ready,
char *sip, unsigned &sport,
char *oip, unsigned &oport,
char *newip, unsigned &newport){
	std::vector<struct AddressChange>::iterator i;
	unsigned j;
	for(i = this->listening.begin(); i != this->listening.end(); i++){
		for(j = 0; j != (*i).count; j++){
			if((*i).sfd[j] == ready)
				break;
		}
		if(j != (*i).count)
			break;
	}
	if(i == this->listening.end()){
		return 0;
	}
STDCOUT("addresschange: fd matched\n");
	const int rasize = sizeof(struct ReplyAddress);
	struct ReplyAddress ra;
	if(read(ready, &ra, rasize) != rasize){
STDCOUT("addresschange: error\n");
		this->fs->unregisterFD(ready);
		close(ready);
		(*i).sfd[j] = -1;
		(*i).livingcount--;
		if((*i).livingcount == 0){
			// this->listening.erase(i);
		}
		return -1;
	}

	if(strcmp((*i).name, ra.name) != 0){
		std::cerr << "addresschange error: name not match " << (*i).name << ", " << ra.name << "\n";
		return -1;
	}
	if(strcmp((*i).oldip, ra.ip) == 0 && (*i).oldport == ra.port){
STDCOUT("addresschange: no change\n");
		return 2;
	}

	strcpy(newip, ra.ip);
	newport = ra.port;
	strcpy(oip, (*i).oldip);
	oport = (*i).oldport;
	strcpy(sip, (*i).srcip);
	sport = (*i).srcport;
STDCOUT("addresschange: " << sip << ":" << sport << "->" <<
newip << ":" << newport << "(" << oip << ":" << oport << ")\n");
	closeAll(*i, *(this->fs));
	// this->listening.erase(i);
	return 1;
}


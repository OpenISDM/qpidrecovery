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
	for(unsigned i = 0; i != ac.count; i++){
		strcpy(ac.proposers[i], rp.ip[i]);
		ac.sfd[i] = tcpconnect(ac.proposers[i], QUERY_BACKUP_PORT);
		if(ac.sfd[i] >= 0){
			this->fs->registerFD(ac.sfd[i]);
			ac.livingcount++;
		}
		else
			ac.sfd[i] = -1;
	}
	strcpy(ac.oldip, oip);
	ac.oldport = oport;
	strcpy(ac.srcip, sip);
	ac.srcport = sport;

	if(ac.livingcount == 0)
		return -1;
	this->listening.push_back(ac);
	return 0;
}

static void closeAll(struct AddressChange &ac, FileSelector &fs){
	for(unsigned i = 0; i != ac.count; i++){
		if(ac.sfd[i] >= 0){
			fs.unregisterFD(ac.sfd[i]);
			close(ac.sfd[i]);
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
STDCOUT("addresschange: no matched fd\n");
		return 0;
	}

	const int rasize = sizeof(ReplyAddress);
	struct ReplyAddress ra;
	if(read(ready, &ra, rasize) != rasize){
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
		std::cerr << "addresschange error: name not match\n";
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

	closeAll(*i, *(this->fs));
	// this->listening.erase(i);
	return 1;
}


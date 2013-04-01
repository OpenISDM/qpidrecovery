const char *discoverIP(){
	return "127.0.0.1";
}
const char *discoverAcceptor(){
	return "127.0.0.1";
}
const char *discoverProposer(){
	return "127.0.0.1";
}

struct{
	char *ip;
	unsigned port;
}backuplist[] = {
	{"127.0.0.1", 5673},
	{"127.0.0.1", 5674}
};
const char *discoverBackup(unsigned &port){
	static int a = 0;
	a++;
	port = backuplist[a-1].port;
	return backuplist[a-1].ip;
}


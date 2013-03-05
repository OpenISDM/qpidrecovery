
int getLocalIP(char* ip);

int udpsocket(unsigned port);

int udpreceivefrom(int sfd, char* from, void* buf, int bufsize);

int udpsendto(int sfd, const char* ipaddress, unsigned port, void* buf, int bufsize);
int tcpserversocket(unsigned port);
int tcpaccept(int sfd, char* from);

/*create a tcp client and connect, return socket*/
int tcpconnect(const char*ipaddr, unsigned port);

/*int shutdownRDWR(int sfd);*/

/*
SIGPIPE comes when writing a closed socket
default action is terminating
*/
void replaceSIGPIPE();

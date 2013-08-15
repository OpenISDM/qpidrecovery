int readMonitoredBrokerArgument(int argc, const char*argv[]);
int readBackupBrokerArgument(int argc, const char*argv[]);
const char *getSubnetBroker();

const char *getAcceptor(const char *servicename);
const char *getProposer(const char *servicename);
const char *getBackupBroker(const char*failip, unsigned &port, int &score);
bool checkFailure(const char *checkip);

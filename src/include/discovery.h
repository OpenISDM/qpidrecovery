bool isCentralizedMode();
//const char *discoverIP();
const char *discoverAcceptor(const char *servicename);
const char *discoverProposer(const char *servicename);
const char *discoverBackup(const char *localip, const char*failip,
unsigned &port, int &score);
bool isActiveInExperiment(const char *servicename);
bool checkFailure(const char *checkip);

#define NBROKER (50)
#define FAIL_TIME (110)


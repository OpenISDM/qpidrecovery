/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

int readMonitoredBrokerArgument(int argc, const char*argv[]);
int readBackupBrokerArgument(int argc, const char*argv[]);
int readProposerArgument(int argc, const char*argv[]);
int readAcceptorArgument(int argc, const char*argv[]);

bool checkFailure(const char *checkip);

const char *getBackupBrokerURL(const char*failip, unsigned failport, int &score);
const char *getSubnetBrokerURL();
const char *getAcceptorIP();
const char *getProposerIP();

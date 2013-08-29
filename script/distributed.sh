 # this is an example for initialization -> failure -> recovery

#!/bin/bash
BROKERIP=127.0.0.1
BROKERPORT=5672
BACKUPIP=127.0.0.1
BACKUPPORT=5673
PROPOSERIP=127.0.0.1
ACCEPTORIP=127.0.0.1

# initialization on PROPOSER
heartbeat_server &
sleep 1
proposer backuplist.txt &

# initialization on ACCEPTOR
if [ ${ACCEPTORIP} != ${PROPOSERIP} ]; then
	heartbeat_server &
fi
acceptor &

# initialization on BROKER
qpidd -p ${BROKERPORT} --no-data-dir --auth no -d
if [ ${BROKERIP} != ${PROPOSERIP} ] && [ ${BROKERIP} != ${ACCEPTORIP} ]; then
	heartbeat_server &
fi
requestor broker001 ${BROKERPORT} 1 1 paxoslist.txt paxoslist.txt &
sleep 1
qpid-config add queue recovery_test -b ${BROKERIP}":"${BROKERPORT}

# initialization on BACKUP
qpidd -p ${BACKUPPORT} --no-data-dir --auth no -d

sleep 9

# failure on BROKER
killall requestor
killall heartbeat_server
qpidd -p ${BROKERPORT} -q
sleep 7

# show result
qpid-stat -q -b ${BACKUPIP}":"${BACKUPPORT}
sleep 3

# cleanup
killall proposer
killall acceptor
qpidd -p ${BACKUPPORT} -q

# this is an example for initialization -> failure -> recovery

#!/bin/bash
BROKERIP=127.0.0.1
BROKERPORT=5672
BACKUPIP=127.0.0.1
BACKUPPORT=5673
LANDMARKIP=127.0.0.1

# initialization on LANDMARK
heartbeat_server &
sleep 1

# initialization on BROKER
qpidd -p ${BROKERPORT} --no-data-dir --auth no -d
if [ ${BROKERIP} != ${LANDMARKIP} ]; then
	heartbeat_server &
fi
requestor --landmark ${LANDMARKIP} &
sleep 1
qpid-config add queue recovery_test -b ${BROKERIP}":"${BROKERPORT}

# initialization on BACKUP
qpidd -p ${BACKUPPORT} --no-data-dir --auth no -d

# initialization on LANDMARK
landmark brokerlist.txt backuplist.txt &

sleep 3
# failure on BROKER
killall requestor
killall heartbeat_server
qpidd -p ${BROKERPORT} -q
sleep 7

# show result
qpid-stat -q -b ${BACKUPIP}":"${BACKUPPORT}
sleep 3

# cleanup
killall landmark
qpidd -p ${BACKUPPORT} -q

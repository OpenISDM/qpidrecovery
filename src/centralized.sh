#!/bin/bash
BROKERIP=127.0.0.1
LANDMARKIP=127.0.0.1

#on LANDMARKIP
if [ ${BROKERIP} != ${LANDMARKIP} ]; then
	./heartbeat_server &
fi

# on BROKERIP
qpidd -p 5672 --no-data-dir --auth no -d
./heartbeat_server &
./requestor --landmark ${LANDMARKIP} &

# on LANDMARKIP
./landmark brokerlist.txt brokerlist.txt &

sleep 5
killall requestor
killall landmark
killall heartbeat_server
qpidd -p 5672 -q
exit

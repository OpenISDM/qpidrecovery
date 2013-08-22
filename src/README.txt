	Qpid Recovery Program README.txt

"make" to compile the source code
"make clean" to delete executable and object files

This software requires Qpid C++ client library and QMF C++ library.
The executables files are "landmark", "proposer", "acceptor", "requestor", "heartbeat_server", "messagecopy_server".
"bgtraffic", "qpidconf", "heartbeat_tester", and "pingd_src/pingd" in "extra" directory


	Centralized Mode:

To execute landmark mode, the required files are "landmark", "requestor", "heartbeat_server"

Before starting landmark, the heartbeat_server processes have to be started on both landmark and broker node.
In addition, it is necessary to start requestor processes along with the brokers to be monitored.

The fillowing command starts heartbeat_server. No parameter is needed.
	# heartbeat_server

The following command starts requestor.
	# requestor --landmark LANDMARK_IP
The LANDMARK_IP is the ip address, not including port, of the landmark that monitors this broker.

After starting Qpid broker, heartbeat_server, and requestor, type the fillowing command to start landmark:
	# landmark BROKER_LIST_FILE BACKUP_LIST_FILE

BROKER_LIST_FILE is the file containing the list of the brokers to be monitored. It is a text file consists of several lines. Each line is in IP:PORT format.
BACKUP_LIST_FILE contains the list of brokers serving as backup. The file format is the same as BROKER_LIST_FILE.
The default arguments of landmark are
	# landmark brokerlist.txt brokerlist.txt

centralized.sh is a sample script.


	Distributed Mode:

To execute distributed mode, the required files are "proposer", "acceptor", "heartbeat_server", "requestor"

The heartbeat_server must run on the nodes with proposer, acceptor, or requestor service.
	# heartbeat_server

In distributed mode, the proposers and acceptors should be started before the requestors.

The following commands start proposer and acceptor. Both of them need no argument.
	# proposer
	# acceptor

The following commands starts requestor.
	# requestor SERVICE_NAME BROKER_PORT NUMBER_OF_PROPOSERS NUNBER_OF_ACCEPTORS PROPOSER_LIST_FILE ACCEPTOR_LIST_FILE

SERVICE_NAME is an unique name of the broker service.
BROKER_PORT is the port of the monitored broker. Because the requestor should run on the node with Qpid broker, the IP address of the broker is omitted.
NUMBER_OF_PROPOSERS is the expected number of proposers that participate in the paxos algorithm.
NUMBER_OF_ACCEPTORS is the expected number of acceptors that participate in the paxos algorithm.
PROPOSER_LIST_FILE is the file containing the list of the available proposers. It is a text file consists of several lines. Each line is an IP address, not including port.
ACCEPTOR_LIST_FILE is the file containing the list of the available acceptors. The file format is the same as PROPOSER_LIST_FILE.
There is no default value for SERVICE_NAME. The default values for other arguments are
	# requestor SERVICE_NAME 5672 0 0 paxoslist.txt paxoslist.txt


	Extra

qpidconf is a simple qmf commandline tool written in C++. Because QMF tools in Python may not be available on platforms other than x86, this program is useful when porting Qpid.

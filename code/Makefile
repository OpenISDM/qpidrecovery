COMMONOBJS=socketlib.o fileselector.o heartbeat_lib.o discovery.o timestamp.o
RECOVERYOBJS=recoverymanager.o brokeraddress.o brokerobject.o listener.o

all: pingd heartbeat requestor acceptor proposer

#sendreceive: sendreceive.cpp
#	g++ sendreceive.cpp -lqpidmessaging -lqpidtypes -lqpidcommon -o sendreceive -Wall -Wextra

requestor: requestor.o $(COMMONOBJS)
	g++ requestor.o $(COMMONOBJS) -o requestor

heartbeat: heartbeat_main.o heartbeat_tester.o $(COMMONOBJS)
	g++ heartbeat_main.o $(COMMONOBJS) -o heartbeat
	g++ heartbeat_tester.o $(COMMONOBJS) -o heartbeat_tester

proposer: proposer.o $(RECOVERYOBJS) $(COMMONOBJS) paxos.o paxosproposal.o
	g++ proposer.o $(COMMONOBJS) paxos.o paxosproposal.o $(RECOVERYOBJS) \
	-lqpidclient -lqmfconsole -lqpidcommon -o proposer

acceptor: acceptor.o $(COMMONOBJS) paxos.o paxosproposal.o
	g++ acceptor.o $(COMMONOBJS) paxos.o paxosproposal.o -o acceptor
	
pingd_lib.o: pingd_src/pingd_lib.c
	g++ -Wall -Wextra -c pingd_src/pingd_lib.c

pingd:
	make pingd -C pingd_src

clean:
	rm *.o heartbeat_tester heartbeat requestor acceptor proposer

.SUFFIX: .o.cpp
.cpp.o:
	g++ -Wall -Wextra -c $<

.SUFFIX: .o.c
.c.o:
	g++ -Wall -Wextra -c $<


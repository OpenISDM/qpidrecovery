COMMONOBJS=common/socketlib.o common/fileselector.o common/timestamp.o setting/namelist.o
HEARTBEATOBJS=heartbeat/heartbeat_lib.o
RECOVERYOBJS=recovery/recoverymanager.o recovery/brokeraddress.o recovery/brokerobject.o recovery/listener.o
PAXOSOBJS=paxos/paxos.o paxos/paxosproposal.o

QPIDLIBS=-lqpidcommon -lqpidclient -lqpidmessaging -lqpidtypes -lqpidclient -pthread
QMFLIBS=-lqmfconsole
INCLUDEDIR=../include

CXX=g++
CXXFLAGS=-Wall -O2 -I$(INCLUDEDIR)

#TARGETS=requestor acceptor proposer landmark heartbeat_server messagecopy_server
TARGETS=landmark heartbeat_server messagecopy_server
EXTRA_TARGETS=extra/qpidconf extra/bgtraffic extra/heartbeat_tester

all: $(TARGETS)

extra: $(EXTRA_TARGETS)

messagecopy_server: messagecopy_server.o messagecopy.o $(COMMONOBJS)
	$(CXX) $^ $(QPIDLIBS) -o $@

requestor: requestor.o $(COMMONOBJS) $(PAXOSOBJS) $(HEARTBEATOBJS)
	$(CXX) $^ -o $@

heartbeat_server: heartbeat_server.o $(COMMONOBJS)
	$(CXX) $^ -o $@

proposer: proposer.o proposer_link.o messagecopy.o $(COMMONOBJS) $(PAXOSOBJS) $(RECOVERYOBJS) $(HEARTBEATOBJS)
	$(CXX) $^ $(QPIDLIBS) $(QMFLIBS) -o $@

landmark: landmark.o proposer_link.o messagecopy.o $(COMMONOBJS) $(PAXOSOBJS) $(RECOVERYOBJS) $(HEARTBEATOBJS)
	$(CXX) $^ $(QPIDLIBS) $(QMFLIBS) -o $@

acceptor: acceptor.o $(COMMONOBJS) $(PAXOSOBJS)
	$(CXX) $^ -o $@

extra/bgtraffic: extra/bgtraffic.o $(COMMONOBJS)
	$(CXX) $^ -o $@

extra/qpidconf: extra/qpidconf.o recovery/brokerobject.o
	$(CXX) $^ $(QPIDLIBS) $(QMFLIBS) -o $@

extra/heartbeat_tester: extra/heartbeat_tester.o $(COMMONOBJS) $(HEARTBEATOBJS)
	$(CXX) $^ -o $@

clean:
	rm *.o */*.o $(TARGETS) #$(EXTRA_TARGETS)

#proposer.o: proposer.cpp
#	$(CXX) $(CXXFLAGS) -c $^ -o $@ -DDISTRIBUTED_MODE

landmark.o: proposer.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@ -DCENTRALIZED_MODE

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

#.SUFFIX: .o.cpp
#.cpp.o:
#	g++ -Wall -c -I../include -O2 $<

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = landmark proposer acceptor requestor heartbeat_server
AM_CFLAGS = -I../include
AM_CPPFLAGS = -I../include

COMMONSRCS = common/socketlib.cpp common/fileselector.cpp common/timestamp.cpp setting/namelist.cpp
HEARTBEATSRCS = heartbeat/heartbeat_lib.cpp
RECOVERYSRCS = recovery/recoverymanager.cpp recovery/brokeraddress.cpp recovery/brokerobject.cpp recovery/listener.cpp
PAXOSSRCS = paxos/paxos.cpp paxos/paxosproposal.cpp

QPIDLIBS = -lqpidcommon -lqpidclient -lqpidmessaging -lqpidtypes -lqpidclient
QMFLIBS = -lqmfconsole
PTHREADFLAGS = -pthread

#messagecopy_server_SOURCES = messagecopy_server.cpp messagecopy.cpp $(COMMONSRCS)
#messagecopy_server_LDADD = $(QPIDLIBS)

requestor_SOURCES = requestor.cpp $(COMMONSRCS) $(PAXOSSRCS) $(HEARTBEATSRCS)

heartbeat_server_SOURCES = heartbeat_server.cpp $(COMMONSRCS)

proposer_SOURCES = proposer.cpp proposer_link.cpp messagecopy.cpp $(COMMONSRCS) $(PAXOSSRCS) $(RECOVERYSRCS) $(HEARTBEATSRCS)
proposer_CPPFLAGS = $(AM_CPPFLAGS) -DDISTRIBUTED_MODE
proposer_LDFLAGS = $(PTHREADFLAGS)

landmark_SOURCES = proposer.cpp proposer_link.cpp messagecopy.cpp $(COMMONSRCS) $(PAXOSSRCS) $(RECOVERYSRCS) $(HEARTBEATSRCS)
landmark_CPPFLAGS = $(AM_CPPFLAGS) -DCENTRALIZED_MODE
landmark_LDFLAGS = $(PTHREADFLAGS)

acceptor_SOURCES = acceptor.cpp $(COMMONSRCS) $(PAXOSSRCS)

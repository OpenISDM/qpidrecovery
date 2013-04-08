#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <iostream>

#include"messagecopy.h"

MessageCopier::MessageCopier(std::string srcbroker, std::string dstbroker){
	this->srcconn = new Connection(srcbroker);
	this->dstconn = new Connection(dstbroker);
	this->srcconn->open();
	this->dstconn->open();

	this->srcsess = srcconn->createSession();
	this->dstsess = dstconn->createSession();
}

MessageCopier::~MessageCopier(){
	delete this->srcconn;
	delete this->dstconn;
}

int MessageCopier::copyQueueMessage(std::string queuename){
	std::string receiveoption, sendoption;
	sendoption.assign(queuename);
	sendoption.append("; {create:never}");
	receiveoption.assign(queuename);
	receiveoption.append("; {create:never, mode:browse}");

	try{
		Receiver receiver = this->srcsess.createReceiver(receiveoption);
        	Sender sender = dstsess.createSender(sendoption);
		try{
			while(1){
				Message msg = receiver.fetch(Duration::SECOND * 0);
				sender.send(msg);
				std::cout << msg.getContent() << std::endl;
			}
		}
		catch(const std::exception &error){
		}
		receiver.close();
		sender.close();
		return 0;
	} catch(const std::exception& error) {
		std::cout << error.what() << std::endl;
		return -1;
	}
}


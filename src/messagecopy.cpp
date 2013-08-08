#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <iostream>

#include"messagecopy.h"
#include"stdlib.h"
#include"timestamp.h"

QpidClient::QpidClient(std::string brokerurl, std::string targeturl){
	this->conn = new Connection(brokerurl);
	this->conn->open();
	this->sess = conn->createSession();
	if(targeturl.compare("") != 0){
		this->conn2 = new Connection(targeturl);
		this->conn2->open();
		this->sess2 = conn2->createSession();
	}
}

QpidClient::~QpidClient(){
	delete this->conn;
}

static const char *randomString(int length){
#define RANDOM_STRING_LENGTH (10000)
	static char s[RANDOM_STRING_LENGTH + 1];
	int i;
	if(length > RANDOM_STRING_LENGTH){
		length = RANDOM_STRING_LENGTH;
	}
	for(i = 0; i < length; i++){
		s[i] = 'a' + i % 26;
	}
	s[i] = '\0';
	return s;
}

int QpidClient::sendRandomString(std::string targetname){
	std::vector<Message> msgvec;
	for(unsigned i = rand()%(100); i < 120 ; i++){
		Message msg(randomString(2048));
		msgvec.push_back(msg);
	}
	return this->send(targetname, msgvec);
}

int QpidClient::send(std::string targetname, std::vector<Message> &msgvec){
	std::string sendoption;
	sendoption.assign(targetname);
	sendoption.append("; {create:never}");
	try{
	        Sender sender = this->sess.createSender(sendoption);
		for(unsigned i = 0; i != msgvec.size(); i++){
			sender.send(msgvec[i]);
		}
		sender.close();
		return 0;
	}catch(const std::exception& error){
		std::cout << error.what() << std::endl;
		return -1;
	}
}

int QpidClient::receive(std::string queuename, std::vector<Message> &msgvec){
	std::string receiveoption;
	receiveoption.assign(queuename);
	receiveoption.append("; {create:never, mode:browse}");
	try{
		Receiver receiver = this->sess.createReceiver(receiveoption);
		try{
			double starttimestamp = getSecond();
			while(1){
				Message msg = receiver.fetch(Duration::SECOND * 4);
				if(getSecond() - starttimestamp > 4)
					break;
				msgvec.push_back(msg);
			}
		}
		catch(const std::exception &error){
		}
		receiver.close();
		return 0;
	}catch(const std::exception& error){
		std::cout << error.what() << std::endl;
		return -1;
	}
}

int QpidClient::receiveSend(std::string sourcename, std::string targetname){
	std::string sendoption, receiveoption;
	sendoption.assign(targetname);
	sendoption.append("; {create:never}");
	receiveoption.assign(sourcename);
	receiveoption.append("; {create:never, mode:browse}");
	try{
	        Sender sender = this->sess2.createSender(sendoption);
		Receiver receiver = this->sess.createReceiver(receiveoption);
		try{
			double starttimestamp = getSecond();
			while(1){
				Message msg = receiver.fetch(Duration::SECOND * 4);
				if(getSecond() - starttimestamp > 4)
					break;
				sender.send(msg);
				//std::cout << msg.getContent() << std::endl;
			}
		}
		catch(const std::exception &error){
		}
		sender.close();
		receiver.close();
		return 0;
	}catch(const std::exception& error){
		std::cout << error.what() << std::endl;
		return -1;
	}
}


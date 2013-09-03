/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include "brokerobject.h"
#include "qpid/console/ConsoleListener.h"
#include "qpid/console/SessionManager.h"
#include <iostream>
#include <cstdio>

using namespace std;
using namespace qpid::console;

unsigned strToUnsigned(string str){
	unsigned r = 0;
	for(unsigned i = 0; i < str.size(); i++){
		r = r * 10 + (str[i] - '0');
	}
	return r;
}

int main ( int argc, char* argv[] ){
     if(argc ==1){
          cout << endl;
          cout<< "./qpidconf add exchange type name broker_ip broker_port" << endl;
          cout << "./qpidconf add queue name broker_ip broker_port" << endl;
          cout << "./qpidconf bind ename qname key broker_ip broker_port" << endl;
          cout << "./qpidconf route dynamic(queue or route) add des_ip des_port src_ip src_port ename qname des_route" << endl;
          return 1;
     }
     string s[argc-1];
     for (int i = 0; i < (argc-1); i++ ){
          s[i] = argv[i+1];
     }
	if ( s[0].compare("add")==0){
		if ( s[1].compare("exchange")==0){
			if ( s[2].compare("direct")==0||s[2].compare("fanout")==0||s[2].compare("topic")==0||s[2].compare("headers")==0){	
				qpid::client::ConnectionSettings settings;
				SessionManager sm;
				Broker* broker;
                                settings.host=s[4];
                                settings.port=strToUnsigned(s[5]);
				broker = sm.addBroker(settings);
				broker ->waitForStable();
				Object::Vector list;
				sm.getObjects(list, "broker");
				if (list.size()==1){
					Object* broker = &(*list.begin());
					ObjectOperator op( broker, OBJ_BROKER );
				     op.addExchange(s[3], s[2]);
					return 0;
				}
				return 1;
			}
               return 1;
		}
          else if (s[1].compare("queue")==0){
               qpid::client::ConnectionSettings settings;
               SessionManager sm;
               Broker* broker;
               settings.host=s[3];
               settings.port=strToUnsigned(s[4]);
               broker = sm.addBroker(settings);
               broker -> waitForStable();
               Object::Vector list;
               sm.getObjects(list, "broker");
               if (list.size()==1){
                    Object* broker = &(*list.begin());
                    ObjectOperator op (broker, OBJ_BROKER);
                    op.addQueue(s[2]);
                    return 0;
               }
               return 1;
          }
          else {
          cout << "wrong usage" << endl;
          return 1;
          }
	} 
     else if (s[0].compare("bind")==0){
           qpid::client::ConnectionSettings settings;
           SessionManager sm;
           Broker* broker;
           settings.host = s[4];
           settings.port = strToUnsigned(s[5]);
           broker = sm.addBroker(settings);
           broker -> waitForStable();
           Object::Vector list;
           sm.getObjects(list, "broker");
           if (list.size()==1){
                Object* broker = &(*list.begin());
                ObjectOperator op (broker, OBJ_BROKER);
                op.addBinding(s[3], s[1], s[2]);
                return 0;
           }
     }
     else if (s[0].compare("route")==0){
          if (s[1].compare("dynamic")==0||s[1].compare("queue")==0||s[1].compare("route")==0){
               if (s[2].compare("add")==0){
                    const bool islocal = (argc - 1 <= 9? true: s[9].compare("des_route") != 0);
                    string src_ip = (islocal? s[5]: s[3]);
                    string dst_ip = (islocal? s[3]: s[5]);
                    string dst_port_str = (islocal? s[4]: s[6]);
                    unsigned dst_port = strToUnsigned(dst_port_str);
                    unsigned src_port = strToUnsigned(islocal? s[6]: s[4]);

                    qpid::client::ConnectionSettings settings;
                    SessionManager sm;
                    Broker* broker;
                    settings.host = src_ip;
                    settings.port = src_port;
                    broker = sm.addBroker(settings);
                    broker -> waitForStable();
                    Object::Vector list;
                    sm.getObjects(list,"broker");
                    if (list.size()==1){
                         Object* broker = &(*list.begin());
                         ObjectOperator op (broker, OBJ_BROKER);
                         op.addLink(dst_ip,dst_port);
                    }
                    else{
                         return 1;
                    }
                    list.clear();
                    sm.getObjects(list,"link");
                    for (Object::Vector::iterator linkiter = list.begin(); linkiter != list.end(); linkiter++){
                         string dst_url = (dst_ip + ":") + dst_port_str;
//cout << linkObjectDest(*linkiter) << " " << dst_url << "\n";
                         if(linkObjectDest(*linkiter).compare(dst_url) != 0)
				continue;
                         Object* link = &(*linkiter);
                         ObjectOperator lk(link, OBJ_LINK);
                         if (s[1].compare("dynamic")==0){
//src, dst, key, isqueue, isdynamic, durable, tag, excludes, islocal, issync
                              lk.addBridge(s[7],s[7],"key",false,true,false,"","",islocal,0);
                         }   
                         else if (s[1].compare("queue")==0){
                              lk.addBridge(s[8], s[7], "key", true, false, false, "", "", islocal, 0);
                         }
                         else if (s[1].compare("route")==0){
                              lk.addBridge(s[7], s[7], "key", false, false, false, "", "", islocal, 0);
                         }
                         else{
                              return 1;
                         }
                         return 0;
                    }
                    
               } 
          }     

     }
     return 1; 
}

/*
** QpidR - Qpid recovery tool
** newslab.csie.ntu.edu.tw, Taiwan
** See Copyright Notice in common.h
*/

#include "pingd_.h"
#include "pingd_lib.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include<errno.h>

struct PingdSample{
    int hops;
    pid_t pid;
};

struct PingdHeader{
    enum{
        PINGD_PING,
        PINGD_SAMPLE,
        PINGD_CHILD
    }type;
    int bodysize;
    union{
        struct PingdPing ping;
        /*struct PingdHops hops;*/
        struct PingdSample sample;
    }body[0];
};

#define HEADERSIZE (sizeof(struct PingdHeader))

char* pingto = NULL;

static int pipefd[2];

static struct PidAddressList{
    pid_t pid;
    int hops;
    char client[MAX_ADDRESS_LENGTH];
    char pingto[MAX_ADDRESS_LENGTH];
    struct PidAddressList* next;
}pidtoaddr[256],* using,* unused;

void pidtoaddr_init(void){
    int a;
    for(a = 0; a < 256 - 1; a++)
        pidtoaddr[a].next = pidtoaddr + a + 1;
    pidtoaddr[a].next = NULL;
    unused = pidtoaddr;
    using = NULL;
}

static int mappid(pid_t p, const char* client, const char* pingto){
    struct PidAddressList* u;
    u = unused;
    if(u == NULL)
        return -1;
    unused = unused->next;
    u->pid = p;
    u->hops = HOPS_ERROR;
    strcpy(u->client, client);
    strcpy(u->pingto, pingto);
    u->next = using;
    using = u;
    return 0;
}

static int sethops(pid_t p, int h){
    struct PidAddressList* u;
    for(u = using; u != NULL; u=u->next){
        if(u->pid == p){
            u->hops = h;
            return 0;
        }
    }
    return -1;
}

static int unmappid(pid_t p, char* client, char* pingto, int* hops){
    struct PidAddressList* u,** u2;
    u2 = &using;
    for(u = using; u != NULL; u = u->next){
        if(p == u->pid){
            *hops = u->hops;
            strcpy(client, u->client);
            strcpy(pingto, u->pingto);
            break;
        }
        u2=&(u->next);
    }
    if(u == NULL)
        return -1;
    (*u2) = u->next;
    u->next = unused;
    unused = u;
    return 0;
}

static void errexit(const char* s){
    fprintf(stderr, "%s\n", s);
    exit(2);
}

static void sigchld_handler(int a){
    struct PingdHeader report;
    report.type = PINGD_CHILD;
    report.bodysize = 0;
    write(pipefd[1], &report, sizeof(report));
}

void pingd_init(){
    struct PingdHeader* report;
    int bodysize = sizeof(struct PingdPing);
    int nullfd, sfd;
    nullfd = open("/dev/null", O_WRONLY);

    if(nullfd == -1)
        errexit("pingd: cannnot open /dev/null");

    if(pipe(pipefd) == -1)
        errexit("pingd: cannot create pipe");

    if(signal(SIGCHLD, sigchld_handler) == SIG_IGN)
        errexit("pingd: cannot register signal handler");
    if((sfd = pingd_socket(0)) == -1)
        errexit("pingd: server socket error");

    pidtoaddr_init();
/*
test_ping("localhost");
test_ping("abcdefg");
test_ping("140.112.28.138");
*/
    report = malloc(bodysize + HEADERSIZE);
    while(1){
        char clientip[MAX_ADDRESS_LENGTH];
        fd_set rset;

        FD_ZERO(&rset);
        FD_SET(pipefd[0],&rset);
        FD_SET(sfd, &rset);
        while(select((pipefd[0] > sfd? pipefd[0]: sfd) + 1, &rset, NULL, NULL, NULL) < 0){
            if(errno != EINTR) /*not interrupted by signal*/
                fprintf(stderr, "pingd: select error\n");
        }
        if(FD_ISSET(pipefd[0], &rset)){
            if(read(pipefd[0], report, HEADERSIZE) != HEADERSIZE)
                errexit("pingd: read pipe error");
            if(bodysize < report->bodysize){
                bodysize = report->bodysize;
                report = realloc(report, bodysize + HEADERSIZE);
            }
            if(read(pipefd[0], report->body, report->bodysize) != report->bodysize)
                errexit("pingd: read pipe error");
        }
        else if(FD_ISSET(sfd, &rset)){
            report->type = PINGD_PING;
            report->bodysize = sizeof(struct PingdPing);
            if(pingd_receivefrom(sfd, clientip, report->body, report->bodysize) == -1){
                fprintf(stderr, "pingd: read socket error\n");
                continue;
            }
        }
        else
            fprintf(stderr, "pingd: select timeout?\n");
        /*select end*/

        switch(report->type){
        case PINGD_SAMPLE:
            {
                struct PingdSample* response = &(report->body->sample);

                if(sethops(response->pid, response->hops) == -1)
                    fprintf(stderr, "pingd: pid set hops error\n");
                break;
            }
        case PINGD_PING:
            {
                pid_t pid = fork();
                if(pid == -1){
                    fprintf(stderr, "pingd: fork error\n");
                    break;
                }
                if(pid == 0){
                    if(dup2(nullfd, 1) == -1)
                        fprintf(stderr, "pingd: dup2 error\n");
                    if(close(nullfd) == -1)
                        fprintf(stderr, "pingd: close nullfd error\n");
                    pingto = (report->body->ping).to;
                    return;
                }
                if(mappid(pid, clientip, (report->body->ping).to) == -1)
                    fprintf(stderr, "pingd: pid map error\n");
                fprintf(stderr, "request from %s, ping %s\n", clientip, (report->body->ping).to);
                break;
            }
        case PINGD_CHILD:
            {
                int result;
                pid_t pid;
                while((pid = waitpid(-1, &result, WNOHANG)) > 0){
                    struct PingdHops response;
                    char c[MAX_ADDRESS_LENGTH];

                    response.hops = HOPS_ERROR;
                    if(unmappid(pid, c, response.from, &(response.hops)) == -1)
                        errexit("pingd: pid unmap (by pid) error");
                    response.timestamp = time(NULL);
                    if(pingd_sendto(sfd, c, &response, sizeof(response), 0) == -1)
                        fprintf(stderr, "sendto error\n");
                    if(result != 0)
                        printf("ping error: %s\n", response.from);
                    else
                        printf("response from %s TTL=%d\n", response.from, response.hops);
                }
                break;
            }
        default:
            errexit("pingd: receive wrong message\n");
            break;
        }
    }
}

void report_hops(int hops){
    struct{
        struct PingdHeader head;
        struct PingdSample body;
    }report;

    report.head.type = PINGD_SAMPLE;
    report.head.bodysize = sizeof(report.body);
    report.body.hops = hops;
    report.body.pid = getpid();

    write(pipefd[1], &report, sizeof(report));
}


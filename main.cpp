#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <zconf.h>
#include <errno.h>
#include <stdio.h>
#include <string>

#define BUFFSIZE 64
#define TIMEOUT_MS 1500
#define SERVERPORT 50025
using namespace std;
int main(int argc, char *argv[]) {
    int fd;
    if(argc<5){
        perror("Invalid number of arguments");
        return 0;
    }
    char* servname=argv[1];
    int port;
    int portend;
    if(argv[2]<=argv[3]){
        port=atoi(argv[2]);
        portend=atoi(argv[3]);
    }
    else{
        port=atoi(argv[2]);
        portend=atoi(argv[3]);
    }
    char* filename=argv[4];
    FILE *f;
    if(argc==6&&strcmp(argv[5],"-C")==0)
        f=fopen(filename,"w");
    else
        f=fopen(filename,"a");

    struct hostent *hp;
    struct sockaddr_in servaddr;
    socklen_t servsocklen=sizeof(servaddr);
    unsigned char buf[BUFFSIZE];
    /*Set two timers for each session*/
    struct timeval tv1;
    tv1.tv_sec=TIMEOUT_MS/3/1000;
    tv1.tv_usec=1000*(TIMEOUT_MS/3-(TIMEOUT_MS/3/1000)*1000);
    struct timeval tv2;
    tv2.tv_sec=TIMEOUT_MS*2/3/1000;
    tv2.tv_usec=1000*(TIMEOUT_MS*2/3-(TIMEOUT_MS*2/3/1000)*1000);
    struct timeval tvmsg;
    tvmsg.tv_sec=7;
    tvmsg.tv_usec=0;

    hp=gethostbyname(servname);
    if(!hp){
        perror("Host not found");
        return 0;
    }
    //begin send message
    memset(buf,0,BUFFSIZE);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(SERVERPORT);
    memcpy((void *)&servaddr.sin_addr,hp->h_addr_list[0], hp->h_length);
    char msg[BUFFSIZE];
    if((fd=socket(AF_INET,SOCK_DGRAM,0))<0)
        perror("Unable to start UDP scan socket");
    sprintf(msg,"request:%d-%d",port,portend);
    if((sendto(fd, msg, strlen(msg), 0, (struct sockaddr*)&servaddr, servsocklen))<0){
        perror("Message sending error");
        return 0;
    }
    memset(msg,0,BUFFSIZE);
    if((setsockopt(fd,SOL_SOCKET, SO_RCVTIMEO,&tvmsg, sizeof(tvmsg)))<0)
        perror("error setting recv timout");
    if((recvfrom(fd,msg,BUFFSIZE,0,(struct sockaddr*)&servaddr,&servsocklen))<0){
        perror("Server ACK timeout.");
        return 0;
    }
    if(strcmp(msg,"OK")!=0){
        perror("Server did not reply ACK");
        return 0;
    }
    //begin scanning
    printf("Scanning begin:\n");
    char tofile[BUFFSIZE];
    bool inital=true;
    while(port<=portend){
        if(port==SERVERPORT) {//avoid the server message port;
            ++port;
            usleep(1500000);
            continue;
        }
        memset(buf,0,BUFFSIZE);
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family=AF_INET;
        servaddr.sin_port=htons(port);
        memcpy((void *)&servaddr.sin_addr,hp->h_addr_list[0], hp->h_length);
        char* test="testmsg";

        if((fd=socket(AF_INET,SOCK_DGRAM,0))<0)
            perror("Unable to start UDP socket");
        if(inital){//initial wait
            usleep(50000);
            inital=false;
        }
        if((setsockopt(fd,SOL_SOCKET, SO_RCVTIMEO,&tv1, sizeof(tv1)))<0)
            perror("error setting recv timout");
        if((sendto(fd, test, strlen(test), 0, (struct sockaddr*)&servaddr, servsocklen))<0){
            printf("Port No.%d is not reachable.\n",port);
            ++port;
            close(fd);
            continue;
        }
        if((recvfrom(fd,buf, BUFFSIZE, 0, (struct sockaddr*)&servaddr, &servsocklen))<=0){
            if(errno==EAGAIN){
                /*Try second wave*/
                if((setsockopt(fd,SOL_SOCKET, SO_RCVTIMEO,&tv2, sizeof(tv2)))<0)
                    perror("error setting recv timout at second attempt");
                if((recvfrom(fd,buf, BUFFSIZE, 0, (struct sockaddr*)&servaddr, &servsocklen))<=0){
                    printf("Port No.%d not responding second.\n",port);
                }
                else{
                    goto suc;
                }
            }
            else{
                printf("Port No.%d not respongding.\n",port);
                usleep(TIMEOUT_MS*2/3*1000);
            }
            /*Other error, no sleep?*/
            ++port;
            close(fd);
            continue;

        }
        /*printf("Received message:%s\n",buf);*/
        //printf("Port %d is alive.\n", port);
suc:    memset(tofile,0,BUFFSIZE);
        sprintf(tofile,"%d\n", port);
        fputs(tofile,f);
        close(fd);
        ++port;
        close(fd);
        /*Some system has rate limitation*/
        usleep(1200);
    }
    fclose(f);
    return 0;
}
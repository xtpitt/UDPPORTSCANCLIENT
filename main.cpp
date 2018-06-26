#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <zconf.h>
#include <errno.h>
#include <stdio.h>
#include <string>

#define BUFFSIZE 64
#define TIMEOUT_MS 3500
#define SERVERPORT 50025
using namespace std;
int main(int argc, char *argv[]) {
    int fd,streamfd;
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
    struct sockaddr_in servaddr, servudpaddr;
    socklen_t servsocklen=sizeof(servaddr);
    socklen_t servudpsocklen=sizeof(servudpaddr);
    int opt=1;
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
    if((streamfd=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("Unable to start TCP msg socket");
        return -1;
    }

    if(connect(streamfd,(struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        perror("Server is not open");
        return 0;
    }
    sprintf(msg,"request:%d-%d",port,portend);
    if((sendto(streamfd, msg, strlen(msg), 0, (struct sockaddr*)&servaddr, servsocklen))<0){
        perror("Message sending error");
        return 0;
    }
    printf("Request sent:%d-%d\n",port,portend);
    memset(msg,0,BUFFSIZE);
    if((recvfrom(streamfd,msg,BUFFSIZE,0,(struct sockaddr*)&servaddr,&servsocklen))<0){
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
    char* test=new char;
    bool inital=true;
    while(port<=portend){
        /*if(port==SERVERPORT) {//avoid the server message port;
            ++port;
            usleep(1500000);
            continue;
        }*/
        //hear from TCP:
        memset(msg,0,BUFFSIZE);
        if((recvfrom(streamfd,msg,BUFFSIZE,0,(struct sockaddr*)&servaddr,&servsocklen))<0){
            perror("TCP message link broken during scan.\n");
            return -1;
        }
        if(strcmp(msg,"end")==0){
            fclose(f);
            printf("Server says end.\n");
            printf("Last scanned port %d.\n", port);
            return 0;
        }else{
            port=atoi(msg);
            if(port>portend||port<=0)
                continue;
            //printf("Scanning Port %d\n", port);
        }
        memset(buf,0,BUFFSIZE);
        memset(&servudpaddr, 0, sizeof(servudpaddr));
        servudpaddr.sin_family=AF_INET;
        servudpaddr.sin_port=htons(port);
        memcpy((void *)&servudpaddr.sin_addr,hp->h_addr_list[0], hp->h_length);

        int testlen=0;
        testlen=sprintf(test,"testmsg:%d",port);

        if((fd=socket(AF_INET,SOCK_DGRAM,0))<0)
            perror("Unable to start UDP socket");
        if((setsockopt(fd,SOL_SOCKET, SO_RCVTIMEO,&tv1, sizeof(tv1)))<0)
            perror("error setting recv timout");
        if((sendto(fd, test, testlen, 0, (struct sockaddr*)&servudpaddr, servudpsocklen))<0){
            printf("Port No.%d is not reachable.\n",port);
            //++port;
            close(fd);
            continue;
        }
        if((recvfrom(fd,buf, BUFFSIZE, 0, (struct sockaddr*)&servudpaddr, &servudpsocklen))<=0){
            //++port;
            close(fd);
            continue;

        }
        /*printf("Received message:%s\n",buf);*/
        //printf("Message in port %d: %s \n",port,buf);
        //printf("Port %d is alive.\n", port);
        memset(tofile,0,BUFFSIZE);
        sprintf(tofile,"%d\n", port);
        fputs(tofile,f);
        close(fd);
        if(port==portend)
            break;
        /*Some system has rate limitation*/
    }
    delete test;
    printf("Scanning Request Ends.\n");
    fclose(f);
    close(streamfd);
    return 0;
}
#include "../inc/ns.h"
#include "../../cmn_inc.h"
#include <sys/socket.h>
int send_to_SS(char *buff,char *ss_ip,int ss_port,int size){
    int ss_sock;
    struct sockaddr_in ss_addr;
    if((ss_sock = socket(AF_INET,SOCK_STREAM,0))<0){
        printf("Failed to create a socket\n");
        return -1;
    }
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);

    if (inet_pton(AF_INET,ss_ip,&ss_addr.sin_addr) <= 0){
        printf("Invalid IP\n");
        close(ss_sock);
        return -1;
    }

    if(connect(ss_sock,(struct sockaddr *)&ss_addr,sizeof(ss_addr))<0){
        printf("Connection Failed\n");
        close(ss_sock);
        return -1;
    }

    if(send(ss_sock,buff,size,0) < 0){
        printf("Sending failed\n");
        close(ss_sock);
        return -1;
    }
    printf("Sent the create command to the ss\n");
    char recv_buffer[1024];
    if(recv(ss_sock,recv_buffer,1023,0)<0){
        printf("Error in recieving packet\n");
        close(ss_sock);
        return -1;    
    }
    //unpacking logic
    uint32_t flag;
    char *cmd_str;

    Unpack(recv_buffer,&flag,&cmd_str);
    close(ss_sock);
    if(flag == Success)
        return 0;
    else
        return -1;
}

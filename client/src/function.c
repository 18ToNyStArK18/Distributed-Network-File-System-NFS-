
#include "../inc/client_funcs.h"

void parsing(char *inp_cmd,command_str* command_struct){
    // initializing the command_struct
    memset(command_struct,0,sizeof(command_str));
    int inp_size = strlen(inp_cmd);
    int i = 0;
    while(i < inp_size){
        while(i < inp_size && inp_cmd[i]==' ')
            i++;
        int start = i;
        if(i > inp_size || inp_cmd[i] == '\n' || inp_cmd[i] == '\0')
            break;
        while(i < inp_size && inp_cmd[i] != ' ' && inp_cmd[i] != '\n')
            i++;
        strncpy(command_struct->cmd[command_struct->n],inp_cmd+start,i-start);
        command_struct->n++;
    }
    return;
}

void print_parsed(command_str *command_struct){
    int n = command_struct->n;
    for(int i=0;i<n;i++)
        printf("%dth argument : %s\n",i,command_struct->cmd[i]);
}
int client_ss_write(char *ip , int port , char *filename,int line_idx){
    int ss_sock;
    struct sockaddr_in ss_addr;
    if((ss_sock = socket(AF_INET,SOCK_STREAM,0))<0){
        printf("Failed to create a socket\n");
        return -1;
    }
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (inet_pton(AF_INET,ip,&ss_addr.sin_addr) <= 0){
        printf("Invalid IP\n");
        close(ss_sock);
        return -1;
    }

    if(connect(ss_sock,(struct sockaddr *)&ss_addr,sizeof(ss_addr))<0){
        printf("Connection Failed\n");
        close(ss_sock);
        return -1;
    }
    char buffer[BUFFER_SIZE],send_buff[BUFFER_SIZE];
    Packet pkt;
    uint32_t send_net_len,resp_len,resp_len_net;
    int bytes_to_send;
    sprintf(buffer,"%s %d",filename,line_idx);
    pkt.REQ_FLAG = WRITE_REQ;
    strcpy(pkt.req_cmd,buffer);
    bytes_to_send = Pack(&pkt,send_buff);
    send_net_len = htonl(bytes_to_send);
    send_all(ss_sock, &send_net_len, sizeof(uint32_t));
    send_all(ss_sock,send_buff,bytes_to_send);
    //sent the first line with the filename and the line indx
    int flag = 1;
    while(flag){
        char line[1024];
        fgets(line,1023,stdin);
        if(line[0]=='E')
            flag = 0;
        pkt.REQ_FLAG = WRITE_DATA;
        strcpy(pkt.req_cmd,line);
        bytes_to_send = Pack(&pkt,send_buff);
        send_net_len = htonl(bytes_to_send);
        send_all(ss_sock,&send_net_len,sizeof(uint32_t));
        send_all(ss_sock,send_buff,bytes_to_send);
    }

    recv_all(ss_sock,&resp_len_net,sizeof(uint32_t));
    resp_len = ntohl(resp_len_net);
    recv_all(ss_sock,buffer,resp_len);
    uint32_t flag2;
    char *cmd_str;
    Unpack(buffer,&flag2,&cmd_str);
    if(flag == Success){
        printf("Write executed Successfully\n");
        return 1;
    }
    printf("Write executed Unsuccessfully\n");
    return -1;
}
int client_ss_stream(char *buffer,char *ip , int port,int size){
    int ss_sock;
    struct sockaddr_in ss_addr;
    if((ss_sock = socket(AF_INET,SOCK_STREAM,0))<0){
        printf("Failed to create a socket\n");
        return -1;
    }
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (inet_pton(AF_INET,ip,&ss_addr.sin_addr) <= 0){
        printf("Invalid IP\n");
        close(ss_sock);
        return -1;
    }

    if(connect(ss_sock,(struct sockaddr *)&ss_addr,sizeof(ss_addr))<0){
        printf("Connection Failed\n");
        close(ss_sock);
        return -1;
    }

    uint32_t net_len = htonl(size);
    if (send_all(ss_sock, &net_len, sizeof(net_len)) <= 0) {
        printf("Sending length failed\n");
        close(ss_sock);
        return -1;
    }
    if (send_all(ss_sock, buffer, size) <= 0) {
        printf("Sending data failed\n");
        close(ss_sock);
        return -1;
    }

    printf("Sent file request to SS\n");

    while (1) {

        // Read packet length
        uint32_t resp_len_net;
        if (recv_all(ss_sock, &resp_len_net, sizeof(resp_len_net)) <= 0) {
            printf("Connection closed while reading\n");
            close(ss_sock);
            return -1;
        }

        uint32_t resp_len = ntohl(resp_len_net);
        if (resp_len > BUFFER_SIZE) {
            printf("Packet too large\n");
            close(ss_sock);
            return -1;
        }

        // Read full packet
        char recv_buffer[BUFFER_SIZE];
        if (recv_all(ss_sock, recv_buffer, resp_len) <= 0) {
            printf("Failed to receive complete packet\n");
            close(ss_sock);
            return -1;
        }

        uint32_t flag;
        char *cmd_str;
        Unpack(recv_buffer, &flag, &cmd_str);

        if (flag == STREAM_END)
            break;

        printf("%s", cmd_str);
        fflush(stdout);
    }

    close(ss_sock);
    return 0;
}
int client_ss_read(char *buffer,char *ip , int port,int size){
    int ss_sock;
    struct sockaddr_in ss_addr;
    if((ss_sock = socket(AF_INET,SOCK_STREAM,0))<0){
        printf("Failed to create a socket\n");
        return -1;
    }
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (inet_pton(AF_INET,ip,&ss_addr.sin_addr) <= 0){
        printf("Invalid IP\n");
        close(ss_sock);
        return -1;
    }

    if(connect(ss_sock,(struct sockaddr *)&ss_addr,sizeof(ss_addr))<0){
        printf("Connection Failed\n");
        close(ss_sock);
        return -1;
    }

    uint32_t net_len = htonl(size);
    if (send_all(ss_sock, &net_len, sizeof(net_len)) <= 0) {
        printf("Sending length failed\n");
        close(ss_sock);
        return -1;
    }
    if (send_all(ss_sock, buffer, size) <= 0) {
        printf("Sending data failed\n");
        close(ss_sock);
        return -1;
    }

    printf("Sent file request to SS\n");

    while (1) {

        // Read packet length
        uint32_t resp_len_net;
        if (recv_all(ss_sock, &resp_len_net, sizeof(resp_len_net)) <= 0) {
            printf("Connection closed while reading\n");
            close(ss_sock);
            return -1;
        }

        uint32_t resp_len = ntohl(resp_len_net);
        if (resp_len > BUFFER_SIZE) {
            printf("Packet too large\n");
            close(ss_sock);
            return -1;
        }

        // Read full packet
        char recv_buffer[BUFFER_SIZE];
        if (recv_all(ss_sock, recv_buffer, resp_len) <= 0) {
            printf("Failed to receive complete packet\n");
            close(ss_sock);
            return -1;
        }

        uint32_t flag;
        char *cmd_str;
        Unpack(recv_buffer, &flag, &cmd_str);

        if (flag == READ_END)
            break;

        printf("%s", cmd_str);
    }

    close(ss_sock);
    return 0;
}

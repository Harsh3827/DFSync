
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8001
#define BUFFER_SIZE 1024

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    return sock;
}

int main() {
    char command[BUFFER_SIZE];
    int sock;

    while(1) {
        printf("w25clients$ ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strlen(command)-1] = '\0';

        if(strlen(command) == 0) continue;

        sock = connect_to_server();
        send(sock, command, strlen(command), 0);

        char buffer[BUFFER_SIZE];
        recv(sock, buffer, BUFFER_SIZE, 0);
        printf("%s\n", buffer);

        close(sock);
    }

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 5002
#define BUFFER_SIZE 1024

void handle_upload(int client_socket, const char* filename, const char* dest_path) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "~/S2/%s/%s", dest_path, filename);
    
    // Create directory if it doesn't exist
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "~/S2/%s", dest_path);
    mkdir(dir_path, 0777);

    FILE* file = fopen(full_path, "wb");
    if(file == NULL) {
        perror("File creation failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("File %s received and stored\n", filename);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(1);
    }

    if(listen(server_socket, 5) == 0) {
        printf("S2 Server listening on port %d\n", PORT);
    } else {
        perror("Listen failed");
        exit(1);
    }

    while(1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        
        char buffer[BUFFER_SIZE];
        char command[20];
        char filename[256];
        char dest_path[512];

        recv(client_socket, buffer, BUFFER_SIZE, 0);
        sscanf(buffer, "%s %s %s", command, filename, dest_path);

        if(strcmp(command, "uploadf") == 0) {
            handle_upload(client_socket, filename, dest_path);
        }

        close(client_socket);
    }

    return 0;
}

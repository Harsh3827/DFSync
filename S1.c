
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
#include <errno.h>

#define PORT 8001
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    char command[20];
    char filename[256];
    char dest_path[512];

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if(bytes_received <= 0) {
            break;
        }

        sscanf(buffer, "%s %s %s", command, filename, dest_path);

        if(strcmp(command, "uploadf") == 0) {
            // Handle file upload based on extension
            char *ext = strrchr(filename, '.');
            if(ext != NULL) {
                if(strcmp(ext, ".c") == 0) {
                    // Store locally
                    // Implementation for storing .c files
                } else if(strcmp(ext, ".pdf") == 0) {
                    // Forward to S2
                    // Implementation for forwarding .pdf files
                } else if(strcmp(ext, ".txt") == 0) {
                    // Forward to S3
                    // Implementation for forwarding .txt files
                } else if(strcmp(ext, ".zip") == 0) {
                    // Forward to S4
                    // Implementation for forwarding .zip files
                }
            }
        }
        // Add implementations for other commands
    }
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

    if(listen(server_socket, MAX_CLIENTS) == 0) {
        printf("S1 Server listening on port %d\n", PORT);
    } else {
        perror("Listen failed");
        exit(1);
    }

    while(1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);

        if(fork() == 0) {
            close(server_socket);
            prcclient(client_socket);
            exit(0);
        }
        close(client_socket);
    }

    return 0;
}

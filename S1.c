
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>

#define PORT 5001
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

void forward_file(const char* filename, const char* dest_path, int dest_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Forward connection failed");
        return;
    }

    snprintf(buffer, BUFFER_SIZE, "uploadf %s %s", filename, dest_path);
    send(sock, buffer, strlen(buffer), 0);

    FILE* file = fopen(filename, "rb");
    if(file) {
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(sock, buffer, bytes, 0);
            }
        }
        fclose(file);
        unlink(filename);
    }
    close(sock);
}

void handle_upload(char* filename, char* dest_path, int client_socket) {
    char* ext = strrchr(filename, '.');
    char buffer[BUFFER_SIZE];
    char full_path[512];
    
    snprintf(full_path, sizeof(full_path), "~/S1/%s", dest_path);
    mkdir(full_path, 0777);

    FILE* file = fopen(filename, "wb");
    if(file) {
        int bytes_received;
        while((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, file);
        }
        fclose(file);

        if(strcmp(ext, ".pdf") == 0) {
            forward_file(filename, dest_path, 8002);
        } else if(strcmp(ext, ".txt") == 0) {
            forward_file(filename, dest_path, 8003);
        } else if(strcmp(ext, ".zip") == 0) {
            forward_file(filename, dest_path, 8004);
        }
        
        send(client_socket, "File uploaded successfully", 25, 0);
    }
}

void handle_download(char* filename, int client_socket) {
    char* ext = strrchr(filename, '.');
    char buffer[BUFFER_SIZE];
    FILE* file;

    if(strcmp(ext, ".c") == 0) {
        file = fopen(filename, "rb");
    } else {
        // Request file from appropriate server
        int server_port = 0;
        if(strcmp(ext, ".pdf") == 0) server_port = 8002;
        else if(strcmp(ext, ".txt") == 0) server_port = 8003;
        else if(strcmp(ext, ".zip") == 0) server_port = 8004;

        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
        snprintf(buffer, BUFFER_SIZE, "downlf %s", filename);
        send(server_sock, buffer, strlen(buffer), 0);

        file = fopen("temp_file", "wb");
        while((recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, strlen(buffer), file);
        }
        fclose(file);
        close(server_sock);
        file = fopen("temp_file", "rb");
    }

    if(file) {
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(client_socket, buffer, bytes, 0);
            }
        }
        fclose(file);
        if(strcmp(ext, ".c") != 0) unlink("temp_file");
    }
}

void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    char command[20];
    char arg1[256];
    char arg2[512];

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if(bytes_received <= 0) break;

        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        if(strcmp(command, "uploadf") == 0) {
            handle_upload(arg1, arg2, client_socket);
        } else if(strcmp(command, "downlf") == 0) {
            handle_download(arg1, client_socket);
        }
        // Add other commands here
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

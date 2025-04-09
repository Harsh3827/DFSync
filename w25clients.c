
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 5001
#define BUFFER_SIZE 4096

int verify_file_type(const char* filename) {
    char* ext = strrchr(filename, '.');
    if(!ext) return 0;
    
    return (strcmp(ext, ".c") == 0 ||
            strcmp(ext, ".pdf") == 0 ||
            strcmp(ext, ".txt") == 0 ||
            strcmp(ext, ".zip") == 0);
}

int verify_path(const char* path) {
    return strncmp(path, "~S1/", 4) == 0;
}

void send_command(const char* command) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return;
    }

    send(sock, command, strlen(command), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        // Process response
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    }

    close(sock);
}

void upload_file(const char* filename, const char* dest_path) {
    if(!verify_file_type(filename)) {
        printf("Error: Invalid file type\n");
        return;
    }

    if(!verify_path(dest_path)) {
        printf("Error: Invalid destination path\n");
        return;
    }

    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "uploadf %s %s", filename, dest_path);
    send_command(command);
}

void download_file(const char* filename) {
    if(!verify_path(filename)) {
        printf("Error: Invalid file path\n");
        return;
    }

    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downlf %s", filename);
    send_command(command);
}

void remove_file(const char* filename) {
    if(!verify_path(filename)) {
        printf("Error: Invalid file path\n");
        return;
    }

    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "removef %s", filename);
    send_command(command);
}

void download_tar(const char* filetype) {
    if(strcmp(filetype, ".c") != 0 && 
       strcmp(filetype, ".pdf") != 0 && 
       strcmp(filetype, ".txt") != 0) {
        printf("Error: Invalid file type\n");
        return;
    }

    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downltar %s", filetype);
    send_command(command);
}

void display_filenames(const char* pathname) {
    if(!verify_path(pathname)) {
        printf("Error: Invalid path\n");
        return;
    }

    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "dispfnames %s", pathname);
    send_command(command);
}

int main() {
    char command[20];
    char arg1[256];
    char arg2[512];

    printf("Welcome to Distributed File System Client\n");

    while(1) {
        printf("w25clients$ ");
        scanf("%s", command);

        if(strcmp(command, "uploadf") == 0) {
            scanf("%s %s", arg1, arg2);
            upload_file(arg1, arg2);
        }
        else if(strcmp(command, "downlf") == 0) {
            scanf("%s", arg1);
            download_file(arg1);
        }
        else if(strcmp(command, "removef") == 0) {
            scanf("%s", arg1);
            remove_file(arg1);
        }
        else if(strcmp(command, "downltar") == 0) {
            scanf("%s", arg1);
            download_tar(arg1);
        }
        else if(strcmp(command, "dispfnames") == 0) {
            scanf("%s", arg1);
            display_filenames(arg1);
        }
        else {
            printf("Error: Unknown command\n");
        }
    }

    return 0;
}

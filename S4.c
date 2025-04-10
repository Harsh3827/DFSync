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

#define SERVER_PORT 8004    // S4 listens on port 8004
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Create directories recursively if they do not exist.
void create_path_if_not_exist(const char *path)
{
    char temp[512];
    strcpy(temp, path);
    char *p = temp + 1; // skip first character
    while ((p = strchr(p, '/')) != NULL)
    {
        *p = '\0';
        mkdir(temp, 0777);
        *p = '/';
        p++;
    }
    mkdir(temp, 0777);
}

void get_s4_folder_path(char *base_path)
{
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        snprintf(base_path, 512, "%s/S4_folder", cwd);
        mkdir(base_path, 0777);
    }
    else
    {
        perror("getcwd() error");
        exit(1);
    }
}

void sanitize_path(char *resolved_path, const char *raw_path, const char *base_dir)
{
    // Strip "~S1/" if present; S1 sends the destination using ~S1/ prefix.
    if (strncmp(raw_path, "~S1/", 4) == 0)
    {
        snprintf(resolved_path, 512, "%s/%s", base_dir, raw_path + 4);
    }
    else
    {
        snprintf(resolved_path, 512, "%s", raw_path);
    }
}

void upload_handler(int client_socket, char *filename, char *dest_path)
{
    char *ext = strrchr(filename, '.');
    if (!ext)
    {
        printf("Invalid file extension.\n");
        return;
    }
    
    // Receive file size (sent by S1)
    int filesize;
    recv(client_socket, &filesize, sizeof(int), 0);
    printf("Receiving file: %s (%d bytes)\n", filename, filesize);
    
    char full_path[512];
    
    if (strcmp(ext, ".zip") == 0)
    {
        // Construct full destination path for the .zip file.
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);
        create_path_if_not_exist(dest_path);
        
        FILE *fp = fopen(full_path, "wb");
        if (fp == NULL)
        {
            perror("File open failed");
            return;
        }
        
        int bytes_received, total_received = 0;
        char buffer[BUFFER_SIZE];
        while (total_received < filesize)
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
                break;
            fwrite(buffer, 1, bytes_received, fp);
            total_received += bytes_received;
        }
        fclose(fp);
        printf("File saved to %s\n", full_path);
        send(client_socket, "File stored in S4 successfully", 30, 0);
    }
    else
    {
        printf("Forwarding %s to appropriate server based on extension\n", filename);
    }
}

void prcclient(int client_socket)
{
    char buffer[BUFFER_SIZE];
    char command[20], filename[256], path[512];
    
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
            break;
        
        sscanf(buffer, "%s %s %s", command, filename, path);
        
        if (strcmp(command, "uploadf") == 0)
        {
            char base_path[512];
            get_s4_folder_path(base_path);
            
            char dest_path[512];
            sanitize_path(dest_path, path, base_path);
            
            upload_handler(client_socket, filename, dest_path);
        }
        else
        {
            printf("Received unknown command: %s\n", buffer);
            send(client_socket, "Unknown command", 15, 0);
        }
    }
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }
    
    if (listen(server_socket, MAX_CLIENTS) == 0)
    {
        printf("S4 Server listening on port %d\n", SERVER_PORT);
    }
    else
    {
        perror("Listen failed");
        exit(1);
    }
    
    while (1)
    {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }
        if (fork() == 0)
        {
            close(server_socket);
            prcclient(client_socket);
            exit(0);
        }
        close(client_socket);
    }
    
    return 0;
}

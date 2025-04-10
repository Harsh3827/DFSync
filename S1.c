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

#define S1_PORT 8001
#define S3_PORT 8003      // for .txt files
#define S4_PORT 8004      // for .zip files
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

void create_path_if_not_exist(const char *path)
{
    char temp[512];
    strcpy(temp, path);
    char *p = temp + 1; // skip first slash
    while ((p = strchr(p, '/')) != NULL)
    {
        *p = '\0';
        mkdir(temp, 0777);
        *p = '/';
        p++;
    }
    mkdir(temp, 0777); // Create final directory
}

void get_s1_folder_path(char *base_path)
{
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        snprintf(base_path, 512, "%s/S1_folder", cwd);
        mkdir(base_path, 0777); // ensure S1_folder exists
    }
    else
    {
        perror("getcwd() error");
        exit(1);
    }
}

void sanitize_path(char *resolved_path, const char *raw_path, const char *base_dir)
{
    if (strncmp(raw_path, "~S1/", 4) == 0)
    {
        snprintf(resolved_path, 512, "%s/%s", base_dir, raw_path + 4);
    }
    else
    {
        snprintf(resolved_path, 512, "%s", raw_path);
    }
}

int connect_to_server(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(1);
    }
    return sock;
}

/*
 * upload_handler()
 * Parameters:
 *   client_socket: socket connected to the client
 *   filename: name of file (e.g. sample.txt or sample.zip)
 *   dest_path: resolved destination path for .c files (local storage)
 *   full_cmd: full command line as received from the client (e.g. "uploadf sample.zip ~S1/folder")
 */
void upload_handler(int client_socket, char *filename, char *dest_path, char full_cmd[])
{
    char *ext = strrchr(filename, '.');
    if (!ext)
    {
        printf("Invalid file extension.\n");
        return;
    }
    
    // Receive file size from client
    int filesize;
    recv(client_socket, &filesize, sizeof(int), 0);
    printf("Receiving file: %s (%d bytes)\n", filename, filesize);
    
    char full_path[512];
    
    if (strcmp(ext, ".c") == 0)
    {
        // Save .c files locally on S1
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
            fwrite(buffer, 1, bytes_received, fp);
            total_received += bytes_received;
        }
        fclose(fp);
        printf("File saved to %s\n", full_path);
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        // Forward .txt file to S3
        int sock = connect_to_server(S3_PORT);
        // Send the complete command line to S3
        send(sock, full_cmd, strlen(full_cmd), 0);
        usleep(100000); // small delay for safety
        
        // Send file size
        send(sock, &filesize, sizeof(int), 0);
        usleep(100000);
        
        int bytes_received, total_received = 0;
        char buffer[BUFFER_SIZE];
        while (total_received < filesize)
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                printf("Error receiving file from client\n");
                close(sock);
                return;
            }
            int sent = send(sock, buffer, bytes_received, 0);
            if (sent < bytes_received)
            {
                printf("Error forwarding data to S3\n");
                close(sock);
                return;
            }
            total_received += bytes_received;
        }
        char response[BUFFER_SIZE];
        memset(response, 0, BUFFER_SIZE);
        recv(sock, response, BUFFER_SIZE, 0);
        printf("S3 response: %s\n", response);
        close(sock);
        send(client_socket, "File uploaded successfully", 26, 0);
    }
    else if (strcmp(ext, ".zip") == 0)
    {
        // Forward .zip file to S4
        int sock = connect_to_server(S4_PORT);
        // Send the complete command line to S4
        send(sock, full_cmd, strlen(full_cmd), 0);
        usleep(100000);
        
        // Send file size
        send(sock, &filesize, sizeof(int), 0);
        usleep(100000);
        
        int bytes_received, total_received = 0;
        char buffer[BUFFER_SIZE];
        while (total_received < filesize)
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                printf("Error receiving file from client\n");
                close(sock);
                return;
            }
            int sent = send(sock, buffer, bytes_received, 0);
            if (sent < bytes_received)
            {
                printf("Error forwarding data to S4\n");
                close(sock);
                return;
            }
            total_received += bytes_received;
        }
        char response[BUFFER_SIZE];
        memset(response, 0, BUFFER_SIZE);
        recv(sock, response, BUFFER_SIZE, 0);
        printf("S4 response: %s\n", response);
        close(sock);
        send(client_socket, "File uploaded successfully", 26, 0);
    }
    else
    {
        printf("Forwarding %s to appropriate server based on extension not implemented\n", filename);
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
        
        char base_path[512];
        get_s1_folder_path(base_path);
        
        char dest_path[512];
        sanitize_path(dest_path, path, base_path);
        
        if (strcmp(command, "uploadf") == 0)
        {
            // Pass the full command (complete command line) to the handler
            upload_handler(client_socket, filename, dest_path, buffer);
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
    server_addr.sin_port = htons(S1_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }
    if (listen(server_socket, MAX_CLIENTS) == 0)
    {
        printf("S1 Server listening on port %d\n", S1_PORT);
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

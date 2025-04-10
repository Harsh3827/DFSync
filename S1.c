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
#define SERVER_PORT_2 8002
#define SERVER_PORT_3 8003
#define SERVER_PORT_4 8004

int connect_to_server(int SERVER_PORT)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(1);
    }

    return sock;
}

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

// forward the .zip , .pdf, .txt file to their respective server
void file_forwader(int sock, char command[], int filesize, int client_socket, char *server_name)
{

    // Forward the command
    send(sock, command, strlen(command), 0);
    usleep(100000); // Delay for safety

    // Forward file size
    send(sock, &filesize, sizeof(int), 0);
    usleep(100000);

    // Receive and forward the entire file
    char buffer[BUFFER_SIZE];
    int bytes_received, total_received = 0;

    while (total_received < filesize)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("Error receiving file from W25client\n");
            close(sock);
            return;
        }

        // Forward data to specific server
        int sent = send(sock, buffer, bytes_received, 0);
        if (sent < bytes_received)
        {
            printf("Error forwarding data to %s\n", server_name);
            close(sock);
            return;
        }

        total_received += bytes_received;
    }

    // Get confirmation from server
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    recv(sock, response, BUFFER_SIZE, 0);
    printf("%s response: %s\n", server_name, response);

    // Close connection to server
    close(sock);

    // Inform client that the upload was successful
    send(client_socket, "File uploaded successfully", 26, 0);
}

void upload_handler(int client_socket, char *filename, char *dest_path, char command[])
{
    char *ext = strrchr(filename, '.');
    if (!ext)
    {
        printf("Invalid file extension.\n");
        return;
    }

    // Receive file size
    int filesize;
    recv(client_socket, &filesize, sizeof(int), 0);
    printf("Receiving file: %s (%d bytes)\n", filename, filesize);

    char full_path[512];

    if (strcmp(ext, ".c") == 0)
    {
        // Save in ~/S1/...
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
    else if (strcmp(ext, ".pdf") == 0) // Note the "== 0" for equality
    {
        // // Connect to S2
        // int sock = connect_to_server(SERVER_PORT_2);
        // if (sock < 0)
        // {
        //     printf("Failed to connect to S2\n");
        //     return;
        // }

        // // Forward the command
        // send(sock, command, strlen(command), 0);
        // usleep(100000); // Delay for safety

        // // Forward file size
        // send(sock, &filesize, sizeof(int), 0);
        // usleep(100000);

        // // Receive and forward the entire file
        // char buffer[BUFFER_SIZE];
        // int bytes_received, total_received = 0;

        // while (total_received < filesize)
        // {
        //     bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        //     if (bytes_received <= 0)
        //     {
        //         printf("Error receiving file from client\n");
        //         close(sock);
        //         return;
        //     }

        //     // Forward data to S2
        //     int sent = send(sock, buffer, bytes_received, 0);
        //     if (sent < bytes_received)
        //     {
        //         printf("Error forwarding data to S2\n");
        //         close(sock);
        //         return;
        //     }

        //     total_received += bytes_received;
        // }

        // // Get confirmation from S2
        // char response[BUFFER_SIZE];
        // memset(response, 0, BUFFER_SIZE);
        // recv(sock, response, BUFFER_SIZE, 0);
        // printf("S2 response: %s\n", response);

        // // Close connection to S2
        // close(sock);

        // // Inform client that the upload was successful
        // send(client_socket, "File uploaded successfully", 26, 0);

        int sock = connect_to_server(SERVER_PORT_2);
        if (sock < 0)
        {
            printf("Failed to connect to S2\n");
            return;
        }

        file_forwader(sock, command, filesize, client_socket, "S2");
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_3);
        if (sock < 0)
        {
            printf("Failed to connect to S2\n");
            return;
        }

        file_forwader(sock, command, filesize, client_socket, "S3");
    }
    else if (strcmp(ext, ".zip") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_4);
        if (sock < 0)
        {
            printf("Failed to connect to S2\n");
            return;
        }

        file_forwader(sock, command, filesize, client_socket, "S4");
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
        {
            break;
        }

        sscanf(buffer, "%s %s %s", command, filename, path);

        // Get dynamic S1 folder path
        char base_path[512];
        get_s1_folder_path(base_path);

        // Resolve ~S1/... to actual full folder path
        char dest_path[512];
        sanitize_path(dest_path, path, base_path);

        if (strcmp(command, "uploadf") == 0)
        {
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
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) == 0)
    {
        printf("S1 Server listening on port %d\n", PORT);
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

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
#define SERVER_PORT_4 8005

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
void download_request_forwader(int server_socket, char buffer[], int client_socket, char *servername)
{

    send(server_socket, buffer, strlen(buffer), 0);
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);

    int filesize;
    recv(server_socket, &filesize, sizeof(int), 0);
    printf("Receiving file: %s (%d bytes)\n", "prerakshah", filesize);
    // Send file size
    send(client_socket, &filesize, sizeof(int), 0);
    usleep(100000);

    char input_buffer[filesize];
    int bytes_received, total_received = 0;

    while (total_received < filesize)
    {
        bytes_received = recv(server_socket, input_buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("Error receiving file from W25client\n");
            close(server_socket);
            return;
        }

        // Forward data to specific server
        int sent = send(client_socket, input_buffer, bytes_received, 0);
        if (sent < bytes_received)
        {
            printf("Error forwarding data to %s\n", servername);
            close(server_socket);
            return;
        }

        total_received += bytes_received;
    }

    printf("%s response: %s\n", servername, response);
    close(server_socket);
    send(client_socket, response, strlen(response), 0);
}
void download_handler(int client_socket, char buffer[])
{
    char command[20], file_path[512];
    sscanf(buffer, "%s %s", command, file_path);

    char *ext = strrchr(file_path, '.');
    if (!ext)
    {
        printf("Invalid file extension in download command.\n");
        send(client_socket, "Invalid file extension", 22, 0);
        return;
    }

    char base_path[512];
    get_s1_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, file_path, base_path);

    if (strcmp(ext, ".c") == 0)
    {
        // For .c files stored locally
        FILE *fp = fopen(resolved_path, "rb");
        if (fp == NULL)
        {
            printf("Cannot open file %s\n", resolved_path);
            send(client_socket, &(int){0}, sizeof(int), 0);
            return;
        }

        // Get file size
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);

        // Send file size
        send(client_socket, &filesize, sizeof(int), 0);
        usleep(100000);

        // Send file content
        char filebuffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = fread(filebuffer, 1, BUFFER_SIZE, fp)) > 0)
        {
            send(client_socket, filebuffer, bytes, 0);
        }

        fclose(fp);
        printf("File '%s' sent to client successfully.\n", resolved_path);
    }
    else if (strcmp(ext, ".pdf") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_2);
        download_request_forwader(sock, buffer, client_socket, "S2");
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_3);
        download_request_forwader(sock, buffer, client_socket, "S3");
    }
    else if (strcmp(ext, ".zip") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_4);
        download_request_forwader(sock, buffer, client_socket, "S4");
    }
    else
    {
        printf("Unsupported file extension: %s\n", ext);
        send(client_socket, &(int){0}, sizeof(int), 0); // Send 0 size to indicate error
    }
}

void remove_request_forwader(int sock, char buffer[], int client_socket, char *servername)
{

    send(sock, buffer, strlen(buffer), 0);
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    recv(sock, response, BUFFER_SIZE, 0);
    printf("%s response: %s\n", servername, response);
    close(sock);
    send(client_socket, response, strlen(response), 0);
}
//---------- New Remove Functionality ----------
void remove_handler(int client_socket, char buffer[])
{
    char command[20], filename[256], file_path[512];
    sscanf(buffer, "%s %s", command, file_path);
    char *ext = strrchr(file_path, '.');
    if (!ext)
    {
        printf("Invalid file extension in remove command.\n");
        send(client_socket, "Invalid file extension", 22, 0);
        return;
    }
    char base_path[512];
    get_s1_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, file_path, base_path);

    if (strcmp(ext, ".c") == 0)
    {
        if (remove(resolved_path) == 0)
        {
            printf("Removed file %s\n", resolved_path);
            send(client_socket, "File removed successfully", 26, 0);
        }
        else
        {
            perror("Error removing file");
            send(client_socket, "Error removing file", 20, 0);
        }
    }
    else if (strcmp(ext, ".pdf") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_2);
        remove_request_forwader(sock, buffer, client_socket, "S2");
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_3);
        remove_request_forwader(sock, buffer, client_socket, "S3");
    }
    else
    {
        send(client_socket, "Unsupported file type for remove", 33, 0);
    }
}

void downltar_handler(int client_socket, char *buffer)
{
    // buffer is the full command, e.g., "downltar .c"
    char command[20], filetype[16];
    sscanf(buffer, "%s %s", command, filetype);
    if(strcmp(filetype, ".c") == 0)
    {
        // Create tar file of all .c files in S1_folder
        char s1folder[512];
        get_s1_folder_path(s1folder);
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand),
                 "find %s -type f -name '*.c' | tar -cf cfiles.tar -T -", s1folder);
        system(tarCommand);
        // Open and send cfiles.tar to client
        FILE *fp = fopen("cfiles.tar", "rb");
        if(fp == NULL)
        {
            perror("Failed to open tar file");
            send(client_socket, "Error creating tar file", 25, 0);
            return;
        }
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes;
        while((bytes = fread(filebuffer, 1, BUFFER_SIZE, fp)) > 0)
        {
            send(client_socket, filebuffer, bytes, 0);
        }
        fclose(fp);
        remove("cfiles.tar");
        printf("Tar file cfiles.tar sent successfully.\n");
    }
    else if(strcmp(filetype, ".pdf") == 0)
    {
        // Forward downltar command to S2
        int sock = connect_to_server(SERVER_PORT_2);
        send(sock, buffer, strlen(buffer), 0);
        int filesize;
        recv(sock, &filesize, sizeof(int), 0);
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes, totalReceived = 0;
        while(totalReceived < filesize)
        {
            bytes = recv(sock, filebuffer, BUFFER_SIZE, 0);
            if(bytes <= 0) break;
            send(client_socket, filebuffer, bytes, 0);
            totalReceived += bytes;
        }
        close(sock);
        printf("Tar file (pdf.tar) received from S2 and forwarded to client.\n");
    }
    else if(strcmp(filetype, ".txt") == 0)
    {
        // Forward downltar command to S3
        int sock = connect_to_server(SERVER_PORT_3);
        send(sock, buffer, strlen(buffer), 0);
        int filesize;
        recv(sock, &filesize, sizeof(int), 0);
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes, totalReceived = 0;
        while(totalReceived < filesize)
        {
            bytes = recv(sock, filebuffer, BUFFER_SIZE, 0);
            if(bytes <= 0) break;
            send(client_socket, filebuffer, bytes, 0);
            totalReceived += bytes;
        }
        close(sock);
        printf("Tar file (text.tar) received from S3 and forwarded to client.\n");
    }
    else
    {
        send(client_socket, "Unsupported file type for downltar", 35, 0);
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

        sscanf(buffer, "%s", command);
        // sscanf(buffer, "%s %s %s", command, filename, path);

        // Get dynamic S1 folder path
        char base_path[512];
        get_s1_folder_path(base_path);

        // Resolve ~S1/... to actual full folder path
        char dest_path[512];
        // sanitize_path(dest_path, path, base_path);

        if (strcmp(command, "uploadf") == 0)
        {
            sscanf(buffer, "%s %s %s", command, filename, path);
            sanitize_path(dest_path, path, base_path);
            upload_handler(client_socket, filename, dest_path, buffer);
        }
        else if (strcmp(command, "downlf") == 0)
        {
            // printf("this is inside the download");
            download_handler(client_socket, buffer);
        }
        else if (strcmp(command, "removef") == 0)
        {
            remove_handler(client_socket, buffer);
        }
        else if (strcmp(command, "downltar") == 0)
        {
            downltar_handler(client_socket, buffer);
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

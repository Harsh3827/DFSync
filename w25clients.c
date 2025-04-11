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
#define MAX_ARGS 5
#define MAX_COMMAND_LENGTH 256

int connect_to_server()
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

void parse_commands(char *input, char *args[MAX_ARGS], const char *separator)
{
    int counter = 0;
    char *token = strtok(input, separator);
    while (token != NULL && counter < MAX_ARGS - 1)
    {
        args[counter++] = token;
        token = strtok(NULL, separator);
    }
    args[counter] = NULL;
}

/*
 * download_tar() - Downloads a tar file from S1.
 * The tar file will be saved locally with a name depending on the file type requested:
 *   .c  -> cfiles.tar
 *   .pdf -> pdf.tar
 *   .txt -> text.tar
 */
void download_tar(int sock, char *file_type)
{
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "downltar %s", file_type);
    send(sock, command, strlen(command), 0);
    
    int filesize;
    recv(sock, &filesize, sizeof(int), 0);
    if (filesize <= 0)
    {
        printf("Error: No tar file or error creating tar archive\n");
        return;
    }
    
    char tar_filename[64];
    if (strcmp(file_type, ".c") == 0)
        strcpy(tar_filename, "cfiles.tar");
    else if (strcmp(file_type, ".pdf") == 0)
        strcpy(tar_filename, "pdf.tar");
    else if (strcmp(file_type, ".txt") == 0)
        strcpy(tar_filename, "text.tar");
    else
        strcpy(tar_filename, "downloaded_tar.tar");
    
    printf("Receiving tar file: %s (%d bytes)\n", tar_filename, filesize);
    
    FILE *fp = fopen(tar_filename, "wb");
    if (fp == NULL)
    {
        perror("File open failed");
        return;
    }
    
    char bufferTar[BUFFER_SIZE];
    int bytes_received, total_received = 0;
    while (total_received < filesize)
    {
        bytes_received = recv(sock, bufferTar, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
            break;
        fwrite(bufferTar, 1, bytes_received, fp);
        total_received += bytes_received;
    }
    fclose(fp);
    printf("Tar file downloaded successfully as: %s\n", tar_filename);
}

void upload_file(int sock, const char *file_name, const char *destination_path)
{
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL)
    {
        printf("Cannot open file %s\n", file_name);
        return;
    }
    
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "uploadf %s %s", file_name, destination_path);
    
    send(sock, command, strlen(command), 0);
    usleep(100000);
    
    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    rewind(fp);
    
    send(sock, &filesize, sizeof(int), 0);
    usleep(100000);
    
    char filebuffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = fread(filebuffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        send(sock, filebuffer, bytes, 0);
    }
    
    fclose(fp);
    printf("File '%s' uploaded successfully.\n", file_name);
}

void download_file(int sock, char buffer[])
{
    send(sock, buffer, strlen(buffer), 0);
    
    int filesize;
    recv(sock, &filesize, sizeof(int), 0);
    
    if (filesize <= 0)
    {
        printf("Error: File not found or empty\n");
        return;
    }
    
    char command[20], file_path[512];
    sscanf(buffer, "%s %s", command, file_path);
    
    const char *basename = strrchr(file_path, '/');
    if (basename)
        basename++;  // skip '/'
    else
        basename = file_path;
    
    printf("Receiving file: %s (%d bytes)\n", basename, filesize);
    
    char local_filepath[512];
    snprintf(local_filepath, sizeof(local_filepath), "%s", basename);
    
    FILE *fp = fopen(local_filepath, "wb");
    if (fp == NULL)
    {
        perror("File open failed");
        return;
    }
    
    char recv_buffer[BUFFER_SIZE];
    int bytes_received, total_received = 0;
    while (total_received < filesize)
    {
        bytes_received = recv(sock, recv_buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
            break;
        fwrite(recv_buffer, 1, bytes_received, fp);
        total_received += bytes_received;
    }
    
    fclose(fp);
    printf("File downloaded successfully to: %s\n", local_filepath);
}

int main()
{
    int sock = connect_to_server();
    
    printf("============================================\n");
    printf("   Welcome to the Distributed File System   \n");
    printf("           Client (w25clients)              \n");
    printf("============================================\n");
    
    char command[BUFFER_SIZE];
    while (1)
    {
        printf("w25clients$ ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "exit") == 0)
        {
            printf("Exiting w25clients. Goodbye!\n");
            break;
        }
        
        char *command_array[MAX_ARGS];
        char temp_command[MAX_COMMAND_LENGTH];
        strcpy(temp_command, command);
        parse_commands(temp_command, command_array, " ");
        
        if (strcmp(command_array[0], "uploadf") == 0)
        {
            if (command_array[1] == NULL || command_array[2] == NULL)
            {
                printf("Usage: uploadf <filename> <destination_path>\n");
                continue;
            }
            upload_file(sock, command_array[1], command_array[2]);
        }
        else if (strcmp(command_array[0], "downlf") == 0)
        {
            download_file(sock, command);
        }
        else if (strcmp(command_array[0], "downltar") == 0)
        {
            // Call our separate function to download a tar file.
            download_tar(sock, command_array[1]);
        }
        else if (strcmp(command_array[0], "removef") == 0)
        {
            send(sock, command, strlen(command), 0);
            memset(command, 0, sizeof(command));
            recv(sock, command, sizeof(command), 0);
            printf("S1 response: %s\n", command);
        }
        else
        {
            send(sock, command, strlen(command), 0);
            memset(command, 0, sizeof(command));
            recv(sock, command, sizeof(command), 0);
            printf("S1 response: %s\n", command);
        }
    }
    
    close(sock);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8001
#define BUFFER_SIZE 1024
#define MAX_ARGS 5
#define MAX_COMMAND_LENGTH 256

<<<<<<< Updated upstream
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

void upload_file(int sock, const char *file_name, const char *destination_path)
{
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL)
    {
        printf("Cannot open file %s\n", file_name);
        return;
    }

    // Construct command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "uploadf %s %s", file_name, destination_path);

    // Send command first
    send(sock, command, strlen(command), 0);
    usleep(100000); // Delay for safety

    // Get file size
    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    rewind(fp);

    // Send file size
    send(sock, &filesize, sizeof(int), 0);
    usleep(100000);

    // Send file content
    char filebuffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = fread(filebuffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        send(sock, filebuffer, bytes, 0);
    }

    fclose(fp);
    printf("File '%s' uploaded.\n", file_name);
}

int main()
{
    int sock = connect_to_server();

    char command[BUFFER_SIZE];
    while (1)
    {
        printf("w25clients$ ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline

        if (strcmp(command, "exit") == 0)
        {
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
        else
        {
            // Send the entire command as is for other operations
            send(sock, command, strlen(command), 0);
            memset(command, 0, sizeof(command));
            recv(sock, command, sizeof(command), 0);
            printf("S1 response: %s\n", command);
        }
=======
// Check that the file has an allowed extension (.c, .pdf, .txt, or .zip)
int verify_file_type(const char* filename) {
    char* ext = strrchr(filename, '.');
    if (!ext)
        return 0;
    return (strcmp(ext, ".c") == 0 ||
            strcmp(ext, ".pdf") == 0 ||
            strcmp(ext, ".txt") == 0 ||
            strcmp(ext, ".zip") == 0);
}

// Ensure destination or file path starts with "~S1/"
int verify_path(const char* path) {
    return (strncmp(path, "~S1/", 4) == 0);
}

// For simple commands (that don't require file transfers) create a connection,
// send the command, and print the server's response.
void send_command_simple(const char* command) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }
    
    send(sock, command, strlen(command), 0);
    
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
>>>>>>> Stashed changes
    }
    close(sock);
<<<<<<< Updated upstream
=======
}

// Upload file: Connects to the server, sends the upload command,
// waits for the "READY" response, then reads and sends the file content.
void upload_file(const char* filename, const char* dest_path) {
    // Validate file type and destination path
    if (!verify_file_type(filename)) {
        printf("Error: Invalid file type\n");
        return;
    }
    if (!verify_path(dest_path)) {
        printf("Error: Invalid destination path. Must start with ~S1/\n");
        return;
    }
    
    // Create the socket and connect to the S1 server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }
    
    // Send the upload command in the format: "uploadf filename dest_path"
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "uploadf %s %s", filename, dest_path);
    send(sock, command, strlen(command), 0);
    
    // Wait for S1 to respond with "READY"
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        printf("No response from server.\n");
        close(sock);
        return;
    }
    buffer[bytes_received] = '\0';
    if (strncmp(buffer, "READY", 5) != 0) {
        printf("Server did not respond with READY, response: %s\n", buffer);
        close(sock);
        return;
    }
    
    // Open the file that is to be uploaded
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        close(sock);
        return;
    }
    
    // Read and send file contents in chunks
    while (!feof(file)) {
        size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes > 0) {
            send(sock, buffer, bytes, 0);
        }
    }
    fclose(file);
    
    // Optionally signal the end of transmission
    shutdown(sock, SHUT_WR);
    
    // Wait for final response from server after file transfer completes
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    }
    close(sock);
}

// Download file command: uses the simple command path
void download_file(const char* filename) {
    if (!verify_path(filename)) {
        printf("Error: Invalid file path. Must start with ~S1/\n");
        return;
    }
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downlf %s", filename);
    send_command_simple(command);
}

// Remove file command: uses the simple command path
void remove_file(const char* filename) {
    if (!verify_path(filename)) {
        printf("Error: Invalid file path. Must start with ~S1/\n");
        return;
    }
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "removef %s", filename);
    send_command_simple(command);
}

// Download tar command: downloads a tar archive for a given file type.
// Only .c, .pdf, and .txt types are supported.
void download_tar(const char* filetype) {
    if (strcmp(filetype, ".c") != 0 && 
        strcmp(filetype, ".pdf") != 0 && 
        strcmp(filetype, ".txt") != 0) {
        printf("Error: Invalid file type for tar download\n");
        return;
    }
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "downltar %s", filetype);
    send_command_simple(command);
}

// Display filenames command: displays list of files in a directory on S1.
void display_filenames(const char* pathname) {
    if (!verify_path(pathname)) {
        printf("Error: Invalid path. Must start with ~S1/\n");
        return;
    }
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "dispfnames %s", pathname);
    send_command_simple(command);
}

int main() {
    char input_command[20];
    char arg1[256];
    char arg2[512];
    
    printf("Welcome to Distributed File System Client\n");
    printf("Available commands: uploadf, downlf, removef, downltar, dispfnames, exit\n");
    
    // Main loop: read commands from the user and execute corresponding functions.
    while (1) {
        printf("w25clients$ ");
        scanf("%s", input_command);
        
        if (strcmp(input_command, "uploadf") == 0) {
            // Expect two arguments: filename and destination path
            scanf("%s %s", arg1, arg2);
            upload_file(arg1, arg2);
        } 
        else if (strcmp(input_command, "downlf") == 0) {
            scanf("%s", arg1);
            download_file(arg1);
        } 
        else if (strcmp(input_command, "removef") == 0) {
            scanf("%s", arg1);
            remove_file(arg1);
        }
        else if (strcmp(input_command, "downltar") == 0) {
            scanf("%s", arg1);
            download_tar(arg1);
        }
        else if (strcmp(input_command, "dispfnames") == 0) {
            scanf("%s", arg1);
            display_filenames(arg1);
        }
        else if (strcmp(input_command, "exit") == 0) {
            break;
        }
        else {
            printf("Error: Unknown command\n");
        }
    }
>>>>>>> Stashed changes
    return 0;
}

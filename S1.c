// S1.c - Main server that routes client requests based on file extensions. Listens on port 7777.

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
#include <time.h>

#define PORT 7777
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define SERVER_PORT_2 7778
#define SERVER_PORT_3 7779
#define SERVER_PORT_4 7780

// Establishes connection to another server (S2, S3, S4) based on provided port
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

// // this sets the S1 folder path usually under the HOME directory eg. home/patel4xa/S1
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

/**
 * Get the path to S1 server's storage directory
 * Creates the S1 directory in the user's home directory if it doesn't exist
 */
void get_s1_folder_path(char *base_path)
{
    const char *home_dir = getenv("HOME");
    char cwd[512];
    if (home_dir != NULL)
    {
        snprintf(base_path, 512, "%s/S1", home_dir);
        mkdir(base_path, 0777); // ensure S1_folder exists
    }
    else
    {
        perror("getcwd() error");
        exit(1);
    }
}

// check whether path exist return 2 for direcotory, return 1 for file, otherwise 0
int check_path_exists(const char *path)
{
    struct stat path_stat;
    // Check if path exists using stat
    if (stat(path, &path_stat) != 0)
    {
        return 0;
    }
    // Check if it's a directory
    if (S_ISDIR(path_stat.st_mode))
    {
        return 2;
    }
    return 1;
}

// Lists all files from a given directory filtered by file extension, results sorted alphabetically
char *list_all_files(const char *path, const char *extension, char *result, size_t result_size)
{
    char command[1024];
    // Clear the result buffer
    memset(result, 0, result_size);

    // Validate inputs
    if (path == NULL || result == NULL || result_size <= 0)
    {
        return NULL;
    }

    // Check if directory exists
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        return NULL;
    }

    // Build the find command
    if (extension != NULL)
    {
        // Filter by extension
        snprintf(command, sizeof(command), "find \"%s\" -type f -name \"*%s\"", path, extension);
    }
    else
    {
        // All files
        snprintf(command, sizeof(command), "find \"%s\" -type f", path);
    }
    // Execute the command
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("popen failed");
        return NULL;
    }
    // Store all filenames in an array for sorting
    char filenames[1000][256];
    int file_count = 0;
    char line[1024];
    // read each and extract names
    while (fgets(line, sizeof(line), fp) != NULL && file_count < 1000)
    {
        line[strcspn(line, "\n")] = 0;

        const char *filename = strrchr(line, '/'); // Extract just the filename (not the full path)
        if (filename)
        {
            filename++; // Skip the '/'
        }
        else
        {
            filename = line;
        }
        // Store the filename
        strncpy(filenames[file_count], filename, 255);
        filenames[file_count][255] = '\0';
        file_count++;
    }
    pclose(fp);
    // Sort the filenames
    for (int i = 0; i < file_count - 1; i++)
    {
        for (int j = 0; j < file_count - i - 1; j++)
        {
            if (strcasecmp(filenames[j], filenames[j + 1]) > 0)
            {
                // Swap filenames
                char temp[256];
                strcpy(temp, filenames[j]);
                strcpy(filenames[j], filenames[j + 1]);
                strcpy(filenames[j + 1], temp);
            }
        }
    }
    // Now add the sorted filenames to the result
    size_t current_size = 0;
    for (int i = 0; i < file_count; i++)
    {
        size_t needed = strlen(filenames[i]) + 1; // +1 for newline
        if (current_size + needed >= result_size - 1)
        {
            break;
        }
        // Append the filename and a newline
        strcat(result, filenames[i]);
        strcat(result, "\n");

        current_size += needed;
    }
    return result;
}

// Converts '~S1/' notation to actual directory path
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
            // close(sock);
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

/* OPTION 2 - Upload file feature ----------------------------------------------------------------*/
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
        send(client_socket, "File uploaded successfully", 26, 0);
    }
    else if (strcmp(ext, ".pdf") == 0)
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

// Forwards file download requests to the appropriate server and sends file to client
void download_request_forwader(int server_socket, char buffer[], int client_socket, char *servername)
{
    send(server_socket, buffer, strlen(buffer), 0);

    // Receive file size from the server
    int filesize;
    recv(server_socket, &filesize, sizeof(int), 0);
    printf("Receiving file: %s (%d bytes)\n", " ", filesize);

    // Send file size to client
    send(client_socket, &filesize, sizeof(int), 0);
    usleep(100000);

    // Receive and forward the entire file
    char input_buffer[BUFFER_SIZE]; // Change to use BUFFER_SIZE, not filesize
    int bytes_received, total_received = 0;

    while (total_received < filesize)
    {
        bytes_received = recv(server_socket, input_buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("Error receiving file from server\n");
            break;
        }

        // Forward data to client
        int sent = send(client_socket, input_buffer, bytes_received, 0);
        if (sent < bytes_received)
        {
            printf("Error forwarding data to client\n");
            break;
        }

        total_received += bytes_received;
    }

    // Don't wait for an additional response after file transfer
    close(server_socket);
}

/* OPTION 3 - Download file feature ----------------------------------------------------------------*/
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

// this fucntion forwards file removal requests to respective servers
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

/* OPTION 4 - Remove file feature ----------------------------------------------------------------*/
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

/* OPTION 5 - download tar file feature ----------------------------------------------------------------*/
void downltar_handler(int client_socket, char *buffer)
{
    // buffer is of the form "downltar <filetype>"
    char command[20], filetype[16];
    sscanf(buffer, "%s %s", command, filetype);
    
    if (strcmp(filetype, ".c") == 0)
    {
        char s1folder[512];
        get_s1_folder_path(s1folder);
        
        // First check if any .c files exist
        char check_command[1024];
        snprintf(check_command, sizeof(check_command), "find \"%s\" -type f -name '*.c' | wc -l", s1folder);
        FILE *check_fp = popen(check_command, "r");
        if (check_fp == NULL) {
            send(client_socket, "Error checking for .c files", 26, 0);
            return;
        }
        
        char count_str[16];
        fgets(count_str, sizeof(count_str), check_fp);
        pclose(check_fp);
        
        int file_count = atoi(count_str);
        if (file_count == 0) {
            // Send 0 as filesize first
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            // Then send the error message
            send(client_socket, "No .c files found to create tar archive", 36, 0);
            return;
        }

        // Generate a unique tar filename by appending the current timestamp.
        char tarFilename[128];
        snprintf(tarFilename, sizeof(tarFilename), "cfiles_%ld.tar", time(NULL));
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand),
                 "find \"%s\" -type f -name '*.c' | tar -cf %s -T -", s1folder, tarFilename);
        
        if (system(tarCommand) != 0) {
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "Error creating tar file", 22, 0);
            return;
        }
        
        // Open and send the tar file to the client.
        FILE *fp = fopen(tarFilename, "rb");
        if (fp == NULL)
        {
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "Error creating tar file", 22, 0);
            return;
        }
        
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);
        
        if (filesize == 0) {
            fclose(fp);
            remove(tarFilename);
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "No files found to create tar archive", 34, 0);
            return;
        }
        
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = fread(filebuffer, 1, BUFFER_SIZE, fp)) > 0)
        {
            send(client_socket, filebuffer, bytes, 0);
        }
        fclose(fp);
        remove(tarFilename);
        printf("Tar file %s sent successfully.\n", tarFilename);
    }
    else if (strcmp(filetype, ".pdf") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_2);
        if (sock < 0) {
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "Error connecting to PDF server", 29, 0);
            return;
        }
        send(sock, buffer, strlen(buffer), 0);
        int filesize;
        recv(sock, &filesize, sizeof(int), 0);
        
        if (filesize <= 0) {
            close(sock);
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "No PDF files found to create tar archive", 37, 0);
            return;
        }
        
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes, totalReceived = 0;
        while (totalReceived < filesize)
        {
            bytes = recv(sock, filebuffer, BUFFER_SIZE, 0);
            if (bytes <= 0)
                break;
            send(client_socket, filebuffer, bytes, 0);
            totalReceived += bytes;
        }
        close(sock);
        printf("Tar file (pdf.tar) received from S2 and forwarded to client.\n");
    }
    else if (strcmp(filetype, ".txt") == 0)
    {
        int sock = connect_to_server(SERVER_PORT_3);
        if (sock < 0) {
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "Error connecting to TXT server", 29, 0);
            return;
        }
        send(sock, buffer, strlen(buffer), 0);
        int filesize;
        recv(sock, &filesize, sizeof(int), 0);
        
        if (filesize <= 0) {
            close(sock);
            int zero_size = 0;
            send(client_socket, &zero_size, sizeof(int), 0);
            send(client_socket, "No TXT files found to create tar archive", 37, 0);
            return;
        }
        
        send(client_socket, &filesize, sizeof(int), 0);
        char filebuffer[BUFFER_SIZE];
        int bytes, totalReceived = 0;
        while (totalReceived < filesize)
        {
            bytes = recv(sock, filebuffer, BUFFER_SIZE, 0);
            if (bytes <= 0)
                break;
            send(client_socket, filebuffer, bytes, 0);
            totalReceived += bytes;
        }
        close(sock);
        printf("Tar file (text.tar) received from S3 and forwarded to client.\n");
    }
    else
    {
        int zero_size = 0;
        send(client_socket, &zero_size, sizeof(int), 0);
        send(client_socket, "Unsupported file type for downltar", 33, 0);
    }
}

// this fucntion get all the files names from servers
void get_fnames_from_other_servers(char *all_file_name, size_t buffer_size, char buffer[], int socket)
{
    int sock = connect_to_server(socket);
    if (sock < 0)
    {
        printf("Failed to connect to server on port %d\n", socket);
        if (buffer_size > 0)
        {
            all_file_name[0] = '\0';
        }
        return;
    }
    send(sock, buffer, strlen(buffer), 0);
    ssize_t bytes_received = recv(sock, all_file_name, buffer_size - 1, 0);
    if (bytes_received >= 0)
    {
        all_file_name[bytes_received] = '\0';
        printf("Server %d response: %s\n\n", socket, all_file_name);
    }
    else
    {
        perror("recv failed");
        if (buffer_size > 0)
        {
            all_file_name[0] = '\0';
        }
        printf("Failed to receive data from server %d\n", socket);
    }
    close(sock);
}

// this will aggregates and sends all complete file listing from all servers
void diplay_filename_handler(int client_socket, char buffer[])
{
    char original_buffer_copy[BUFFER_SIZE];
    strncpy(original_buffer_copy, buffer, BUFFER_SIZE - 1);
    original_buffer_copy[BUFFER_SIZE - 1] = '\0';

    char pdf_files[BUFFER_SIZE * 5] = {0};
    char txt_files[BUFFER_SIZE * 5] = {0};
    char zip_files[BUFFER_SIZE * 5] = {0};
    
    // Get files from all servers
    get_fnames_from_other_servers(pdf_files, sizeof(pdf_files), original_buffer_copy, SERVER_PORT_2);
    get_fnames_from_other_servers(txt_files, sizeof(txt_files), original_buffer_copy, SERVER_PORT_3);
    get_fnames_from_other_servers(zip_files, sizeof(zip_files), original_buffer_copy, SERVER_PORT_4);

    char command[20], filename[256], file_path[512];

    char *token = strtok(buffer, " ");
    char *path_arg = strtok(NULL, "\n");

    strcpy(command, token);
    strcpy(file_path, path_arg);

    char base_path[512];
    get_s1_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, file_path, base_path);

    char file_list[BUFFER_SIZE * 5] = {0};
    if (check_path_exists(resolved_path) == 2)
    {
        if (list_all_files(resolved_path, NULL, file_list, sizeof(file_list)) != NULL)
        {
            printf("All files:\n%s\n", file_list);
        }
    }

    char result[BUFFER_SIZE * 20] = "Files in directory:\n";
    memset(result + strlen(result), 0, sizeof(result) - strlen(result));

    int has_files = 0;

    // Add C files if any
    if (strlen(file_list) > 0)
    {
        strcat(result, file_list);
        has_files = 1;
    }

    // Add PDF files if any
    if (strlen(pdf_files) > 0 && pdf_files[0] != '\0' &&
        strcmp(pdf_files, "path does not exist") != 0 &&
        strcmp(pdf_files, "Error listing files in the specified directory.") != 0)
    {
        strcat(result, pdf_files);
        has_files = 1;
    }

    // Add TXT files if any
    if (strlen(txt_files) > 0 && txt_files[0] != '\0' &&
        strcmp(txt_files, "path does not exist") != 0 &&
        strcmp(txt_files, "Error listing files in the specified directory.") != 0)
    {
        strcat(result, txt_files);
        has_files = 1;
    }

    // Add ZIP files if any
    if (strlen(zip_files) > 0 && zip_files[0] != '\0' &&
        strcmp(zip_files, "path does not exist") != 0 &&
        strcmp(zip_files, "Error listing files in the specified directory.") != 0)
    {
        strcat(result, zip_files);
        has_files = 1;
    }

    if (!has_files) {
        strcpy(result, "No files found in the specified directory.\n");
    }

    // Send the combined result to the client
    send(client_socket, result, strlen(result), 0);

    memset(buffer, 0, strlen(buffer));
}

// Client request processing loop handling different commands
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
        else if (strcmp(command, "dispfnames") == 0)
        {
            // printf("inside the display\n");
            diplay_filename_handler(client_socket, buffer);
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
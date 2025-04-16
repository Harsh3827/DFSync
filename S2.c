// S3.c - Handles pdf file (.pdf) operations on port 7778.

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

// #define PORT 8001
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define SERVER_PORT_2 7778


// this function creates a directory path recursively if not already present.
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

// this Sets the S2 base folder path usually under the HOME directory eg. home/patel4xa/S2
void get_s2_folder_path(char *base_path)
{
    const char *home_dir = getenv("HOME");
    char cwd[512];
    if (home_dir != NULL)
    {
        snprintf(base_path, 512, "%s/S2", home_dir);
        mkdir(base_path, 0777); // ensure S1_folder exists
    }
    else
    {
        perror("getcwd() error");
        exit(1);
    }
}

// Replace ~S1/ with the actual S2 base folder path.
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

// List and sort all files in a directory
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

    // Check if directory exists or not
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        return NULL;
    }

    if (extension != NULL){
        snprintf(command, sizeof(command),"find \"%s\" -type f -name \"*%s\"",path, extension);
    }
    else
    {
        // All files
        snprintf(command, sizeof(command),"find \"%s\" -type f",path);
    }
    // Execute the command
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("popen failed");
        return NULL;
    }
    // Storing name for sorting
    char filenames[1000][256]; // Support up to 1000 files
    int file_count = 0;
    char line[1024];
    // Read each line and extract the filename
    while (fgets(line, sizeof(line), fp) != NULL && file_count < 1000)
    {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        // Extract just the filename (not the full path)
        const char *filename = strrchr(line, '/');
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
        filenames[file_count][255] = '\0'; // Ensure null termination
        file_count++;
    }
    pclose(fp);

    // Sort the filenames using a simple bubble sort
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

        // Check if we have enough space left in the buffer
        if (current_size + needed >= result_size - 1)
        { // -1 for null terminator
            break;
        }
        // Append the filename and a newline
        strcat(result, filenames[i]);
        strcat(result, "\n");

        current_size += needed;
    }
    return result;
}

// Handle file upload & its only accepts PDF files and stores them in S2.
void upload_handler(int client_socket, char *filename, char *dest_path)
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

    if (strcmp(ext, ".pdf") == 0)
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
        // At the end of the PDF handling in upload_handler()
        send(client_socket, "File stored in S2 successfully", 30, 0);
        fclose(fp);
        printf("File saved to %s\n", full_path);
    }
    else
    {
        printf("Forwarding %s to appropriate server based on extension\n", filename);
    }
}


// Handle file removal
void handle_remove(int client_socket, char *filepath)
{
    // filepath should be like "~S1/folder1/folder2/sample.pdf"
    char base_path[512];
    get_s2_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, filepath, base_path);
    if (remove(resolved_path) == 0)
    {
        printf("Removed file %s\n", resolved_path);
        send(client_socket, "File removed successfully", 33, 0);
    }
    else
    {
        perror("Error removing file");
        send(client_socket, "Error removing file from S2", 28, 0);
    }
}

void download_request_forwader(int sock, char buffer[], int client_socket, char *servername)
{

    send(sock, buffer, strlen(buffer), 0);
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    recv(sock, response, BUFFER_SIZE, 0);
    printf("%s response: %s\n", servername, response);
    close(sock);
    send(client_socket, response, strlen(response), 0);
}

//download handler for downloding fucntion
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
    get_s2_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, file_path, base_path);

    if (strcmp(ext, ".pdf") == 0)
    {
        // For .c files stored locally
        FILE *fp = fopen(resolved_path, "rb");
        if (fp == NULL)
        {
            printf("Cannot open file %s\n", resolved_path);
            send(client_socket, &(int){0}, sizeof(int), 0); // Send 0 size to indicate error
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
        printf("File '%s' sent to S1 successfully.\n", resolved_path);
    }
    else
    {
        printf("Unsupported file extension: %s\n", ext);
        send(client_socket, &(int){0}, sizeof(int), 0); // Send 0 size to indicate error
    }
}

//// Display file names in a directory
void diplay_filename_handler(int client_socket, char buffer[])
{

    char command[20], filename[256], file_path[512];

    char *token = strtok(buffer, " ");
    char *path_arg = strtok(NULL, "\n");

    strcpy(command, token);
    strcpy(file_path, path_arg);

    char base_path[512];
    get_s2_folder_path(base_path);
    char resolved_path[512];
    sanitize_path(resolved_path, file_path, base_path);

    // printf("absolute path  %s\n", resolved_path);
    char file_list[4096] = {0};
    if (check_path_exists(resolved_path) == 2)
    {

        // printf("the path is valid and so inside the conditions\n\n");
        if (list_all_files(resolved_path, NULL, file_list, sizeof(file_list)) != NULL)
        {
            printf("All files:\n%s\n", file_list);
        }
        else
        {
            strcpy(file_list, "Error listing files in the specified directory.");
            printf("Error listing files in %s\n", resolved_path);
        }
    }
    else
    {
        // Path doesn't exist
        strcpy(file_list, "path does not exist");
        printf("Path does not exist: %s\n", resolved_path);
    }

    file_list[4095] = '\0';
    // printf("\n\n%sn\n", resolved_path);
    send(client_socket, file_list, strlen(file_list), 0);
}

//tar function download
void downtar_pdf_fucntion(int client_socket, char buffer[])
{
    char command[20], filetype[16];
    sscanf(buffer, "%s %s", command, filetype);

    if (strcmp(filetype, ".pdf") == 0)
    {
        char s2folder[512];
        get_s2_folder_path(s2folder);
        char tarFilename[128];
        snprintf(tarFilename, sizeof(tarFilename), "pdf_%ld.tar", time(NULL));
        char tarCommand[1024];
        snprintf(tarCommand, sizeof(tarCommand), "find \"%s\" -type f -name '*.pdf' | tar -cf %s -T -", s2folder, tarFilename);
        system(tarCommand);
        FILE *fp = fopen(tarFilename, "rb");
        if (fp == NULL)
        {
            perror("Failed to open tar file");
            send(client_socket, "Error creating tar file", 25, 0);
            return;
        }
                        
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);
        send(client_socket, &filesize, sizeof(int), 0);
        char bufferTar[BUFFER_SIZE];
        int bytes;
        while ((bytes = fread(bufferTar, 1, BUFFER_SIZE, fp)) > 0)
        {
            send(client_socket, bufferTar, bytes, 0);
        }
        fclose(fp);
        remove(tarFilename);
        printf("Tar file %s sent successfully from S2.\n", tarFilename);
    }
    else
    {
        send(client_socket, "Unsupported file type for downltar", 35, 0);
    }
}


// Process client commands
void prcclient(int client_socket)
{
    char buffer[BUFFER_SIZE];
    char command[20], filename[256], path[512];
    ;

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            break;
        }

        sscanf(buffer, "%s %s %s", command, filename, path);

        // get dynamic S1 folder path
        char base_path[512];
        get_s2_folder_path(base_path);

        // Resolve ~S1/... to actual full folder path
        char dest_path[512];
        sanitize_path(dest_path, path, base_path);

        //upload
        if (strcmp(command, "uploadf") == 0)
        {
            upload_handler(client_socket, filename, dest_path);
        }

        // Download a file from the server
        else if (strcmp(command, "downlf") == 0)
        {
            // printf("this is inside the download");
            download_handler(client_socket, buffer);
        }

        //remove fun
        else if (strcmp(command, "removef") == 0)
        {
            handle_remove(client_socket, filename);
        }

        //download tar function
        else if (strcmp(command, "downltar") == 0)
        {
            downtar_pdf_fucntion(client_socket,buffer);
        }

        //display names
        else if (strcmp(command, "dispfnames") == 0)
        {
            printf("inside the display\n");
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
    char filetype[10]; // Added filetype variable declaration

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
    server_addr.sin_port = htons(SERVER_PORT_2);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) == 0)
    {
        printf("S2 Server listening on port %d\n", SERVER_PORT_2);
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
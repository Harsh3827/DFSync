#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
<<<<<<< Updated upstream
=======
#include <sys/stat.h>
#include <sys/wait.h>
>>>>>>> Stashed changes
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
<<<<<<< Updated upstream
#include <errno.h>

#define PORT 8001
#define BUFFER_SIZE 1024
=======
#include <dirent.h>
#include <errno.h>

#define PORT 5001
#define S2_PORT 5002
#define S3_PORT 5003
#define S4_PORT 5004
#define BUFFER_SIZE 4096
>>>>>>> Stashed changes
#define MAX_CLIENTS 10
#define BASE_DIR "~/S1"

<<<<<<< Updated upstream
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
    else
    {
        // Save temporarily, then forward to S2/S3/S4 (to be implemented)
        // We'll just print for now
        printf("Forwarding %s to appropriate server based on extension\n", filename);
        // In final version: Connect to S2/S3/S4 and send filename, dest_path, and file data
=======
// Helper function to create directories recursively
void create_dir_recursive(const char* path) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

// Determine file type and corresponding server port
int get_server_port(const char* filename) {
    char* ext = strrchr(filename, '.');
    if(!ext) return 0;
    
    if(strcmp(ext, ".pdf") == 0) return S2_PORT;
    if(strcmp(ext, ".txt") == 0) return S3_PORT;
    if(strcmp(ext, ".zip") == 0) return S4_PORT;
    
    return 0; // For .c files, handle locally
}

// Extract filename from path
char* get_filename_from_path(const char* path) {
    char* last_slash = strrchr(path, '/');
    if(last_slash) return last_slash + 1;
    return (char*)path;
}

// Convert S1 path to corresponding server path
char* convert_path(const char* path, const char* server_prefix) {
    static char new_path[512];
    
    // Replace ~S1 with appropriate server prefix
    if(strncmp(path, "~S1/", 4) == 0) {
        snprintf(new_path, sizeof(new_path), "%s/%s", server_prefix, path + 4);
    } else {
        snprintf(new_path, sizeof(new_path), "%s/%s", server_prefix, path);
    }
    
    return new_path;
}

// Forward a file to another server
void forward_file(const char* filename, const char* dest_path, int dest_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("Socket creation failed for forwarding");
        return;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Forward connection failed");
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    char* just_filename = get_filename_from_path(filename);
    
    // Remove ~S1 from path if present
    char clean_path[512];
    if(strncmp(dest_path, "~S1/", 4) == 0) {
        strcpy(clean_path, dest_path + 4);
    } else {
        strcpy(clean_path, dest_path);
    }
    
    snprintf(buffer, BUFFER_SIZE, "uploadf %s %s", just_filename, clean_path);
    send(sock, buffer, strlen(buffer), 0);
    
    // Give the server time to process the command
    usleep(50000); // 50ms
    
    FILE* file = fopen(filename, "rb");
    if(file) {
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(sock, buffer, bytes, 0);
            }
        }
        fclose(file);
        unlink(filename); // Delete file from S1 after forwarding
    } else {
        perror("Failed to open file for forwarding");
    }
    
    close(sock);
}

// Handle uploadf command
void handle_upload(int client_socket, char* filename, char* dest_path) {
    char* ext = strrchr(filename, '.');
    if(!ext) {
        send(client_socket, "Error: Invalid file type", 24, 0);
        return;
    }
    
    char buffer[BUFFER_SIZE];
    char full_path[512];
    
    // Remove ~S1 from path if present
    char clean_path[512];
    if(strncmp(dest_path, "~S1/", 4) == 0) {
        strcpy(clean_path, dest_path + 4);
    } else {
        strcpy(clean_path, dest_path);
>>>>>>> Stashed changes
    }
    
    // Create local directory structure
    snprintf(full_path, sizeof(full_path), "%s/%s", BASE_DIR, clean_path);
    create_dir_recursive(full_path);
    
    // Create full path for the file
    char file_full_path[512];
    snprintf(file_full_path, sizeof(file_full_path), "%s/%s/%s", BASE_DIR, clean_path, filename);
    
    FILE* file = fopen(file_full_path, "wb");
    if(!file) {
        send(client_socket, "Error: Could not create file", 28, 0);
        return;
    }
    
    // Inform client we're ready to receive the file
    send(client_socket, "READY", 5, 0);
    
    // Receive file data from client
    int bytes_received;
    while((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        if(bytes_received < BUFFER_SIZE) break; // Assume end of file
    }
    fclose(file);
    
    int server_port = get_server_port(filename);
    if(server_port > 0) {
        // Forward non-.c files to appropriate server and delete local copy
        forward_file(file_full_path, clean_path, server_port);
    }
    
    send(client_socket, "File uploaded successfully", 25, 0);
}

<<<<<<< Updated upstream
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
            upload_handler(client_socket, filename, dest_path);
        }
        else
        {
            printf("Received unknown command: %s\n", buffer);
=======
// Request file from appropriate server
int request_file_from_server(const char* filename, const char* server_path, int server_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return 0;
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return 0;
    }
    
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "downlf %s", filename);
    send(sock, buffer, strlen(buffer), 0);
    
    return sock;
}

// Handle downlf command
void handle_download(int client_socket, char* filepath) {
    char buffer[BUFFER_SIZE];
    char* filename = get_filename_from_path(filepath);
    char* ext = strrchr(filename, '.');
    
    if(!ext) {
        send(client_socket, "Error: Invalid file type", 24, 0);
        return;
    }
    
    // Clean the path (remove ~S1/ if present)
    char clean_path[512];
    if(strncmp(filepath, "~S1/", 4) == 0) {
        strcpy(clean_path, filepath + 4);
    } else {
        strcpy(clean_path, filepath);
    }
    
    if(strcmp(ext, ".c") == 0) {
        // Handle .c files locally
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", BASE_DIR, clean_path);
        
        FILE* file = fopen(file_path, "rb");
        if(!file) {
            send(client_socket, "Error: File not found", 21, 0);
            return;
        }
        
        // Inform client we're sending the file
        send(client_socket, "SENDING", 7, 0);
        
        // Send file data
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(client_socket, buffer, bytes, 0);
            }
        }
        fclose(file);
    } else {
        // Request file from appropriate server
        int server_port = get_server_port(filename);
        if(server_port <= 0) {
            send(client_socket, "Error: Unsupported file type", 28, 0);
            return;
        }
        
        int server_sock = request_file_from_server(clean_path, clean_path, server_port);
        if(server_sock <= 0) {
            send(client_socket, "Error: Failed to contact server", 31, 0);
            return;
        }
        
        // Inform client we're sending the file
        send(client_socket, "SENDING", 7, 0);
        
        // Forward data from server to client
        int bytes_received;
        while((bytes_received = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            send(client_socket, buffer, bytes_received, 0);
        }
        
        close(server_sock);
    }
}

// Handle removef command
void handle_remove(int client_socket, char* filepath) {
    char* filename = get_filename_from_path(filepath);
    char* ext = strrchr(filename, '.');
    
    if(!ext) {
        send(client_socket, "Error: Invalid file type", 24, 0);
        return;
    }
    
    // Clean the path (remove ~S1/ if present)
    char clean_path[512];
    if(strncmp(filepath, "~S1/", 4) == 0) {
        strcpy(clean_path, filepath + 4);
    } else {
        strcpy(clean_path, filepath);
    }
    
    if(strcmp(ext, ".c") == 0) {
        // Handle .c files locally
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", BASE_DIR, clean_path);
        
        if(unlink(file_path) == 0) {
            send(client_socket, "File removed successfully", 24, 0);
        } else {
            send(client_socket, "Error: File not found or cannot be removed", 42, 0);
        }
    } else {
        // Request deletion from appropriate server
        int server_port = get_server_port(filename);
        if(server_port <= 0) {
            send(client_socket, "Error: Unsupported file type", 28, 0);
            return;
        }
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            send(client_socket, "Error: Failed to contact server", 31, 0);
            close(sock);
            return;
        }
        
        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "removef %s", clean_path);
        send(sock, buffer, strlen(buffer), 0);
        
        // Get response from server
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if(bytes_received > 0) {
            buffer[bytes_received] = '\0';
            send(client_socket, buffer, bytes_received, 0);
        } else {
            send(client_socket, "Unknown error occurred", 22, 0);
        }
        
        close(sock);
    }
}

// Create a tar of files
void handle_download_tar(int client_socket, char* filetype) {
    char buffer[BUFFER_SIZE];
    char command[512];
    
    if(strcmp(filetype, ".c") == 0) {
        // Create tar of all .c files locally
        sprintf(command, "find %s -name \"*.c\" | tar czf ./cfiles.tar -T -", BASE_DIR);
        system(command);
        
        FILE* file = fopen("./cfiles.tar", "rb");
        if(!file) {
            send(client_socket, "Error: Failed to create tar file", 32, 0);
            return;
        }
        
        // Inform client we're sending the tar file
        send(client_socket, "SENDING", 7, 0);
        
        // Send tar file
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(client_socket, buffer, bytes, 0);
            }
        }
        fclose(file);
        unlink("./cfiles.tar");
    } else {
        // Request tar from appropriate server
        int server_port = 0;
        char tar_name[20];
        
        if(strcmp(filetype, ".pdf") == 0) {
            server_port = S2_PORT;
            strcpy(tar_name, "pdf.tar");
        } else if(strcmp(filetype, ".txt") == 0) {
            server_port = S3_PORT;
            strcpy(tar_name, "text.tar");
        } else {
            send(client_socket, "Error: Unsupported file type for tar", 36, 0);
            return;
        }
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            send(client_socket, "Error: Failed to contact server", 31, 0);
            close(sock);
            return;
        }
        
        snprintf(buffer, BUFFER_SIZE, "downltar %s", filetype);
        send(sock, buffer, strlen(buffer), 0);
        
        // Inform client we're sending the tar file
        send(client_socket, "SENDING", 7, 0);
        
        // Forward tar file from server to client
        int bytes_received;
        while((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
            send(client_socket, buffer, bytes_received, 0);
        }
        
        close(sock);
    }
}

// Compare function for qsort
int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// Handle dispfnames command
void handle_display_filenames(int client_socket, char* pathname) {
    char buffer[BUFFER_SIZE];
    char result[BUFFER_SIZE * 4] = {0}; // For storing the combined result
    char* filenames[1000]; // Array to store filenames
    int file_count = 0;
    
    // Clean the path (remove ~S1/ if present)
    char clean_path[512];
    if(strncmp(pathname, "~S1/", 4) == 0) {
        strcpy(clean_path, pathname + 4);
    } else {
        strcpy(clean_path, pathname);
    }
    
    // Get .c files locally
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", BASE_DIR, clean_path);
    
    DIR* dir = opendir(dir_path);
    if(!dir) {
        send(client_socket, "Error: Directory not found", 26, 0);
        return;
    }
    
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_type == DT_REG && strstr(entry->d_name, ".c")) {
            filenames[file_count] = strdup(entry->d_name);
            file_count++;
        }
    }
    closedir(dir);
    
    // Get files from S2, S3, and S4
    char response[BUFFER_SIZE];
    for(int port = S2_PORT; port <= S4_PORT; port++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) >= 0) {
            snprintf(buffer, BUFFER_SIZE, "dispfnames %s", clean_path);
            send(sock, buffer, strlen(buffer), 0);
            
            int bytes_received = recv(sock, response, BUFFER_SIZE, 0);
            if(bytes_received > 0) {
                response[bytes_received] = '\0';
                
                // Parse the response which is a newline-separated list
                char* token = strtok(response, "\n");
                while(token != NULL) {
                    filenames[file_count] = strdup(token);
                    file_count++;
                    token = strtok(NULL, "\n");
                }
            }
        }
        close(sock);
    }
    
    // Sort the filenames
    qsort(filenames, file_count, sizeof(char*), compare_strings);
    
    // Combine the results
    strcpy(result, "Files:\n");
    for(int i = 0; i < file_count; i++) {
        strcat(result, filenames[i]);
        strcat(result, "\n");
        free(filenames[i]);
    }
    
    send(client_socket, result, strlen(result), 0);
}

// Main client processing function
void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    char command[20];
    char arg1[256];
    char arg2[512];

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if(bytes_received <= 0) break;
        
        printf("Received command: %s\n", buffer);
        
        // Parse command and arguments
        memset(command, 0, sizeof(command));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));
        
        sscanf(buffer, "%s %s %s", command, arg1, arg2);
        
        // Process commands
        if(strcmp(command, "uploadf") == 0) {
            handle_upload(client_socket, arg1, arg2);
        } else if(strcmp(command, "downlf") == 0) {
            handle_download(client_socket, arg1);
        } else if(strcmp(command, "removef") == 0) {
            handle_remove(client_socket, arg1);
        } else if(strcmp(command, "downltar") == 0) {
            handle_download_tar(client_socket, arg1);
        } else if(strcmp(command, "dispfnames") == 0) {
            handle_display_filenames(client_socket, arg1);
        } else {
>>>>>>> Stashed changes
            send(client_socket, "Unknown command", 15, 0);
        }
    }
    
    close(client_socket);
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    
    // Create S1 directory if it doesn't exist
    mkdir(BASE_DIR, 0777);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }
<<<<<<< Updated upstream
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

=======
    
    // Allow socket reuse to avoid "address already in use" errors
    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }
    
>>>>>>> Stashed changes
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
<<<<<<< Updated upstream

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) == 0)
    {
=======
    
    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(1);
    }
    
    if(listen(server_socket, MAX_CLIENTS) == 0) {
>>>>>>> Stashed changes
        printf("S1 Server listening on port %d\n", PORT);
    }
    else
    {
        perror("Listen failed");
        exit(1);
    }
<<<<<<< Updated upstream

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
=======
    
    while(1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if(client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected\n");
        
        // Fork a child process to handle client
        pid_t pid = fork();
        if(pid == 0) {
            // Child process
>>>>>>> Stashed changes
            close(server_socket);
            prcclient(client_socket);
            exit(0);
        } else {
            // Parent process
            close(client_socket);
            
            // Clean up zombie processes
            while(waitpid(-1, NULL, WNOHANG) > 0);
        }
<<<<<<< Updated upstream

        close(client_socket);
=======
>>>>>>> Stashed changes
    }
    
    return 0;
}
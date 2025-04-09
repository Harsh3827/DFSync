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
#include <dirent.h>

#define PORT 5002
#define BUFFER_SIZE 4096
#define BASE_DIR "~/S2"

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

// Extract filename from path
char* get_filename_from_path(const char* path) {
    char* last_slash = strrchr(path, '/');
    if(last_slash) return last_slash + 1;
    return (char*)path;
}

// Handle uploadf command
void handle_upload(int client_socket, const char* filename, const char* dest_path) {
    char full_path[512];
    
    // Create directory structure
    snprintf(full_path, sizeof(full_path), "%s/%s", BASE_DIR, dest_path);
    create_dir_recursive(full_path);
    
    // Create full path for the file
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", BASE_DIR, dest_path, filename);
    
    FILE* file = fopen(file_path, "wb");
    if(file == NULL) {
        perror("File creation failed");
        return;
    }
    
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        if(bytes_received < BUFFER_SIZE) break; // Assume end of file
    }
    
    fclose(file);
    printf("File %s received and stored at %s\n", filename, file_path);
}

// Handle downlf command
void handle_download(int client_socket, const char* filepath) {
    char buffer[BUFFER_SIZE];
    char file_path[512];
    
    // Get filename from path
    char* filename = get_filename_from_path(filepath);
    
    // Create full path
    snprintf(file_path, sizeof(file_path), "%s/%s", BASE_DIR, filepath);
    
    FILE* file = fopen(file_path, "rb");
    if(file == NULL) {
        send(client_socket, "Error: File not found", 21, 0);
        return;
    }
    
    // Send file data
    while(!feof(file)) {
        size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
        if(bytes > 0) {
            send(client_socket, buffer, bytes, 0);
        }
    }
    
    fclose(file);
}

// Handle removef command
void handle_remove(int client_socket, const char* filepath) {
    char file_path[512];
    
    // Create full path
    snprintf(file_path, sizeof(file_path), "%s/%s", BASE_DIR, filepath);
    
    if(unlink(file_path) == 0) {
        send(client_socket, "File removed successfully", 24, 0);
    } else {
        send(client_socket, "Error: File not found or cannot be removed", 42, 0);
    }
}

// Handle downltar command
void handle_download_tar(int client_socket, const char* filetype) {
    char buffer[BUFFER_SIZE];
    char command[512];
    
    if(strcmp(filetype, ".pdf") == 0) {
        // Create tar of all .pdf files
        sprintf(command, "find %s -name \"*.pdf\" | tar czf ./pdf.tar -T -", BASE_DIR);
        system(command);
        
        FILE* file = fopen("./pdf.tar", "rb");
        if(!file) {
            send(client_socket, "Error: Failed to create tar file", 32, 0);
            return;
        }
        
        // Send tar file
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, BUFFER_SIZE, file);
            if(bytes > 0) {
                send(client_socket, buffer, bytes, 0);
            }
        }
        
        fclose(file);
        unlink("./pdf.tar");
    } else {
        send(client_socket, "Error: Unsupported file type for this server", 44, 0);
    }
}

// Handle dispfnames command
void handle_display_filenames(int client_socket, const char* pathname) {
    char dir_path[512];
    char result[BUFFER_SIZE] = {0};
    char* filenames[1000]; // Array to store filenames
    int file_count = 0;
    
    // Create directory path
    snprintf(dir_path, sizeof(dir_path), "%s/%s", BASE_DIR, pathname);
    
    DIR* dir = opendir(dir_path);
    if(!dir) {
        send(client_socket, "", 0, 0); // Empty response for non-existent directory
        return;
    }
    
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_type == DT_REG && strstr(entry->d_name, ".pdf")) {
            filenames[file_count] = strdup(entry->d_name);
            file_count++;
        }
    }
    closedir(dir);
    
    // Sort filenames alphabetically
    for(int i = 0; i < file_count - 1; i++) {
        for(int j = i + 1; j < file_count; j++) {
            if(strcmp(filenames[i], filenames[j]) > 0) {
                char* temp = filenames[i];
                filenames[i] = filenames[j];
                filenames[j] = temp;
            }
        }
    }
    
    // Combine filenames into result
    for(int i = 0; i < file_count; i++) {
        strcat(result, filenames[i]);
        strcat(result, "\n");
        free(filenames[i]);
    }
    
    send(client_socket, result, strlen(result), 0);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    
    // Create S2 directory if it doesn't exist
    mkdir(BASE_DIR, 0777);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Allow socket reuse
    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(1);
    }
    
    if(listen(server_socket, 5) == 0) {
        printf("S2 Server listening on port %d\n", PORT);
    } else {
        perror("Listen failed");
        exit(1);
    }
    
    while(1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if(client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected\n");
        
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive command
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if(bytes_received <= 0) {
            close(client_socket);
            continue;
        }
        
        printf("Received command: %s\n", buffer);
        
        // Parse command and arguments
        char command[20] = {0};
        char arg1[256] = {0};
        char arg2[512] = {0};
        
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
            send(client_socket, "Unknown command", 15, 0);
        }
        
        close(client_socket);
    }
    
    return 0;
}
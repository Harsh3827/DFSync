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
void get_Client_PWD(char *base_path)
{
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        snprintf(base_path, 512, "%s/", cwd);
        mkdir(base_path, 0777); // ensure S1_folder exists
    }
    else
    {
        perror("getcwd() error");
        exit(1);
    }
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
void get_actual_filename(const char *file_path, char *basename_buffer, size_t buffer_size)
{
    const char *basename = strrchr(file_path, '/');
    if (basename)
    {
        basename++; // Skip the '/'
    }
    else
    {
        basename = file_path;
    }

    // Copy the basename to the provided buffer
    strncpy(basename_buffer, basename, buffer_size);
    basename_buffer[buffer_size - 1] = '\0'; // Ensure null termination
}
void display_extension_error(const char *command_str)
{
    char command[20] = {0};
    sscanf(command_str, "%s", command);

    if (strcmp(command, "uploadf") == 0)
    {
        printf("Error: File must have a valid extension (.c, .pdf, .txt, .zip)\n");
        printf("Usage: uploadf <filename> <destination_path>\n");
    }
    else if (strcmp(command, "downlf") == 0)
    {
        printf("Error: File must have a valid extension (.c, .pdf, .txt, .zip)\n");
        printf("Usage: downlf <filename>\n");
    }
    else if (strcmp(command, "removef") == 0)
    {
        printf("Error: File must have a valid extension (.c, .pdf, .txt)\n");
        printf("Usage: removef <filename>\n");
    }
    else if (strcmp(command, "downltar") == 0)
    {
        printf("Error: Invalid file type. Supported types are: .c, .pdf, .txt\n");
        printf("Usage: downltar <filetype>\n");
    }
    else
    {
        printf("Error: Invalid command or file extension\n");
    }
}
int validate_extension(const char *command_str)
{
    char command[20] = {0};
    char arg1[512] = {0};
    char arg2[512] = {0};

    // Make a copy of the command string since strtok modifies the string
    char command_copy[BUFFER_SIZE];
    strncpy(command_copy, command_str, BUFFER_SIZE - 1);
    command_copy[BUFFER_SIZE - 1] = '\0';

    // Parse the command and its arguments
    char *token = strtok(command_copy, " ");
    if (token != NULL)
    {
        strncpy(command, token, sizeof(command) - 1);
        token = strtok(NULL, " ");
        if (token != NULL)
        {
            strncpy(arg1, token, sizeof(arg1) - 1);
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                strncpy(arg2, token, sizeof(arg2) - 1);
            }
        }
    }

    // Valid extensions
    const char *valid_extensions[] = {".c", ".pdf", ".txt", ".zip"};
    int num_valid_extensions = 4;

    // For downltar, we don't allow .zip files
    const char *downltar_valid_extensions[] = {".c", ".pdf", ".txt"};
    int num_downltar_valid_extensions = 3;

    // Check for valid paths that start with ~S1/
    if (strcmp(command, "uploadf") == 0)
    {
        // For uploadf, the path is the second argument (arg2)
        if (arg2[0] == '\0' || strncmp(arg2, "~S1/", 4) != 0)
        {
            printf("Error: Destination path must start with '~S1/'\n");
            return 0;
        }

        // Also check the file extension in the first argument
        const char *extension = strrchr(arg1, '.');
        if (!extension)
        {
            return 0; // No extension found
        }

        // Validate against our list of extensions
        for (int i = 0; i < num_valid_extensions; i++)
        {
            if (strcmp(extension, valid_extensions[i]) == 0)
            {
                return 1; // Valid extension and path
            }
        }
        return 0; // Invalid extension
    }
    else if (strcmp(command, "downlf") == 0 || strcmp(command, "removef") == 0)
    {
        // For downlf and removef, check that the path starts with ~S1/
        if (strncmp(arg1, "~S1/", 4) != 0)
        {
            printf("Error: File path must start with '~S1/'\n");
            return 0;
        }
        // Extract the filename from the path
        const char *filename = strrchr(arg1, '/');
        if (filename)
        {
            // Skip the slash
            filename++;
            const char *extension = strrchr(filename, '.');
            if (!extension)
            {
                return 0; // No extension found
            }

            // For these commands, all extensions are valid
            for (int i = 0; i < num_valid_extensions; i++)
            {
                if (strcmp(extension, valid_extensions[i]) == 0)
                {
                    return 1; // Valid extension and path
                }
            }
        }
        return 0; // Invalid filename or extension
    }
    else if (strcmp(command, "downltar") == 0)
    {
        for (int i = 0; i < num_downltar_valid_extensions; i++)
        {
            if (strcmp(arg1, downltar_valid_extensions[i]) == 0)
            {
                return 1; // Valid extension for downltar
            }
        }
        return 0; // Invalid extension for downltar
    }
    else if (strcmp(command, "dispfnames") == 0)
    {
        // For dispfnames, check that the path starts with ~S1/
        if (strncmp(arg1, "~S1/", 4) != 0)
        {
            printf("Error: Directory path must start with '~S1/'\n");
            return 0;
        }
        return 1;
    }

    return 0; // Unknown command or invalid input
}

void download_tar(int sock, char *file_type)
{
    printf("Requesting tar archive of %s files...\n", file_type + 1); // Skip the dot in file_type
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "downltar %s", file_type);
    send(sock, command, strlen(command), 0);

    // Receive file size
    int filesize;
    if (recv(sock, &filesize, sizeof(int), 0) <= 0)
    {
        printf("Error: Failed to receive response from server\n");
        return;
    }
    if (filesize <= 0)
    {
        printf("Error: No tar file or error creating tar archive\n");
        return;
    }

    // Generate appropriate filename with timestamp
    char timestamp[32];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);

    char tar_filename[128];
    if (strcmp(file_type, ".c") == 0)
        snprintf(tar_filename, sizeof(tar_filename), "cfiles_%s.tar", timestamp);
    else if (strcmp(file_type, ".pdf") == 0)
        snprintf(tar_filename, sizeof(tar_filename), "pdf_%s.tar", timestamp);
    else if (strcmp(file_type, ".txt") == 0)
        snprintf(tar_filename, sizeof(tar_filename), "text_%s.tar", timestamp);
    else
        snprintf(tar_filename, sizeof(tar_filename), "downloaded_%s.tar", timestamp);

    printf("Receiving tar file as: %s (%d bytes)\n", tar_filename, filesize);

    FILE *fp = fopen(tar_filename, "wb");
    if (fp == NULL)
    {
        perror("Error: File creation failed");
        return;
    }

    // Receive file data in chunks
    char bufferTar[BUFFER_SIZE];
    int bytes_received, total_received = 0;

    while (total_received < filesize)
    {
        bytes_received = recv(sock, bufferTar, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("Warning: Connection closed before receiving complete file\n");
            break;
        }

        size_t written = fwrite(bufferTar, 1, bytes_received, fp);
        if (written < bytes_received)
        {
            printf("Error: Failed to write all received data to file\n");
            break;
        }

        total_received += bytes_received;

        // Show progress for large files
        if (filesize > BUFFER_SIZE * 10 && total_received % (BUFFER_SIZE * 5) == 0)
        {
            printf("Download progress: %d/%d bytes (%.1f%%)\n",
                   total_received, filesize,
                   ((float)total_received / filesize) * 100);
        }
    }

    fclose(fp);

    // Verify the downloaded file
    struct stat st;
    if (stat(tar_filename, &st) == 0 && st.st_size > 0)
    {
        // Check if it's a valid tar file
        char check_cmd[512];
        snprintf(check_cmd, sizeof(check_cmd), "tar -tf %s >/dev/null 2>&1", tar_filename);

        if (system(check_cmd) == 0)
        {
            printf("Tar file downloaded successfully as: %s (%ld bytes)\n",
                   tar_filename, (long)st.st_size);
            printf("You can extract it with: tar -xf %s\n", tar_filename);
        }
        else
        {
            printf("Warning: The downloaded file does not appear to be a valid tar archive\n");
        }
    }
    else
    {
        printf("Error: Downloaded file verification failed\n");
    }
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

    // Get the basename from the path
    char basename[256];
    get_actual_filename(file_path, basename, sizeof(basename));

    printf("Receiving file: %s (%d bytes)\n", basename, filesize);

    // printf("Receiving file: %s (%d bytes)\n", basename, filesize);

    // Save the file in client's current directory
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
        if (!validate_extension(command))
        {
            display_extension_error(command);
            continue;
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
            if (command_array[1] == NULL)
            {
                printf("Usage: downlf <filename along with path>\n");
                continue;
            }

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
        else if (strcmp(command_array[0], "dispfnames") == 0)
        {
            // to display all the files with same folder structure
            command[strcspn(command, "\n")] = 0;

            send(sock, command, strlen(command), 0);
            memset(command, 0, sizeof(command));
            char answer[2343];
            memset(answer, 0, sizeof(answer));
            ssize_t bytes_received = recv(sock, answer, sizeof(answer) - 1, 0); // Leave space for null term

            if (bytes_received >= 0)
            {
                answer[bytes_received] = '\0';
            }
            else
            {
                perror("recv failed");
                answer[0] = '\0';
            }
            // Check if server sent empty response
            if (strlen(answer) == 0 && bytes_received >= 0)
            {
                printf("Path does not exist or contains no files.\n");
            }
            else if (bytes_received > 0)
            {
                printf("%s\n", answer);
            }
        }
        else if (strcmp(command, "exit") == 0)
        {
            printf("Exiting w25clients. Goodbye!\n");
            break;
        }
        else
        {
            // For any other commands, send to S1.
            send(sock, command, strlen(command), 0);
            memset(command, 0, sizeof(command));
            recv(sock, command, sizeof(command), 0);
            printf("S1 response: %s\n", command);
        }
    }

    close(sock);
    return 0;
}
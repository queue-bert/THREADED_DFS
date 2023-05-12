#include "util.h"

volatile sig_atomic_t flag = 0;

char * cwd = NULL;


int check(int stat, char* message)
{
    if (stat == -1)
    {
        perror(message);
        return 1;
        //exit(1);
    }
    else
    {
        return 0;
    }
}

int connects_to_servers(int** sockets, int* server_tot, char** host_info, int num_servers) {
    struct sockaddr_in server_addr;
    int server_count = 0;

    *sockets = malloc(num_servers * sizeof(int));
    if (*sockets == NULL) {
        perror("Error allocating memory");
        // printf("Error allocating memory\n");
        return -1;
    }

    for (int i = 0; i < num_servers; i++) {
        char host[50];
        int port;
// %49[^:]:%d
        if (sscanf(host_info[i], "%49[^:]:%d", host, &port) != 2) {
            fprintf(stderr, "Invalid host information: %s\n", host_info[i]);
            continue;
        }

        // printf("host : %s, port : %d\n", host, port);

        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            perror("Error creating socket");
            // printf("Error creating socket\n");
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
            perror("Error converting IP address");
            // printf("Error converting IP address\n");
            close(socket_fd);
            continue;
        }

        if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            //
            // printf("Error connecting to server\n");
            close(socket_fd);
            continue;
        }

        (*sockets)[server_count++] = socket_fd;
    }

    *server_tot = server_count;
    return server_count;
}


void put_file(char *buffer, int *bytes_written, int file_descriptor, int bytes) {
    ssize_t result = write(file_descriptor, buffer, bytes);
    if (result != -1) {
        *bytes_written += result;
    }
}

void send_file_to_server(char * server_info, const char * filename, int fp, long * start, long curr_chunk, int chunk) {
    char server_address[16];
    int server_port;

    // printf("index:%d\n", ((mod + chunk) % servernum));

    // int index = ((-mod + chunk) % servernum);
    if (sscanf(server_info, "%49[^:]:%d", server_address, &server_port) != 2) {
        fprintf(stderr, "Invalid server address format\n");
        return;
    }

    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        return;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_address, &(server_addr.sin_addr)) <= 0) {
        perror("Invalid server address");
        close(client_socket);
        return;
    }

    // printf("host : %s , port : %d\n", server_address, server_port);
    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        //
        close(client_socket);
        return;
    }

    char buf[BUFSIZE];
    time_t curr_time;
    time(&curr_time);

    int p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", chunk, curr_time, curr_chunk, filename);
    check(sendall(client_socket, buf, &p_sz), "Error sending request header to client\n");

    sendfile(client_socket, fp, start, curr_chunk);

    close(client_socket);
}

void connect_and_send(int *client_socket_fd) {
    int client_socket = *client_socket_fd;
    char buffer[BUFSIZE + 1];
    // char filename[261];
    int n;
    int msg_size = 0;
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    char command[10];
    char full_filename[261];
    char full_path[1000];

    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO | SO_SNDTIMEO, (void *) &timeout, sizeof(timeout));

    memset(buffer, 0, BUFSIZE + 1);
    msg_size = 0;
    buffer[BUFSIZE] = '\0';
    // char * carriage;
    // int header_size;
    char head[100];
    int chunknum;
    long time_stamp;
    long chunksize = LONG_MAX;
    char fname[100];
    int file = -1;
    int stop = 0;

    while (chunksize != msg_size)
    {
        n = recv(client_socket, buffer, (sizeof(buffer) -1), 0);
        
        if (n == -1) {
            break;
        } else if (n == 0) {
            break;
        }

        char *header_end;
        int header_size;

        if (stop == 0 && (header_end= strstr(buffer, "\r\n\r\n")))
        {
            
            header_size = header_end - buffer + 4;  // +4 to include \r\n\r\n

            strncpy(head, buffer, header_size);
            head[header_size] = '\0';

            sscanf(head, "%s %s", command, full_filename);

            sscanf(full_filename, "%d.%ld.%ld.%s", &chunknum, &time_stamp, &chunksize, fname);
            
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, full_filename);

            if(!strcmp(command,"put"))
            {
                file = open(full_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                stop = 1; 
                put_file(buffer + header_size, &msg_size, file, n - header_size);
            }
            else if(!strcmp(command, "discover"))
            {
                int out_len = 0;
                char file_list[BUFSIZE];
                char cmd[BUFSIZE];
                // have to handle if the file doesn't exist
                sprintf(cmd, "ls %s/*%s* 2>/dev/null", cwd, full_filename);
                FILE *fp = popen(cmd, "r");
                if (fp != NULL) {
                    while (fgets(file_list, sizeof(file_list) - 1, fp) != NULL) {
                        out_len = strlen(file_list);
                        sendall(client_socket, file_list, &out_len);
                    }
                    pclose(fp);
                }
                out_len = 4;
                sendall(client_socket, "\r\n\r\n", &out_len);
                break;
            }
            else if(!strcmp(command,"get"))
            {
                off_t offset = 0;
                snprintf(full_path, sizeof(full_path), "%s/%s", cwd, full_filename);
                // printf("filepath: %s\n", full_path);
                int filefd = open(full_path, O_RDONLY);
                if (filefd != -1) {
                    struct stat st;
                    if (fstat(filefd, &st) == 0) {
                        off_t file_size = st.st_size;

                        while (file_size > 0) {
                            ssize_t sent = sendfile(client_socket, filefd, &offset, BUFSIZ);
                            if (sent <= 0) {
                                perror("Error sending file");
                                break;
                            }
                            file_size -= sent;
                        }
                    }
                    close(filefd);
                }
                else
                {
                    int out_len = 17;
                    sendall(client_socket, "File not found\r\n\r\n", &out_len); // Send error message if file not found
                    break;
                }
                break;
            }
            else if (!strcmp(command,"ls"))
            {
                int out_len = 0;
                char file_list[BUFSIZE];
                char cmd[BUFSIZE];
                // have to handle if the file doesn't exist
                sprintf(cmd, "ls %s 2>/dev/null", cwd);
                FILE *fp = popen(cmd, "r");
                if (fp != NULL) {
                    while (fgets(file_list, sizeof(file_list) - 1, fp) != NULL) {
                        out_len = strlen(file_list);
                        // printf("%s", file_list);
                        sendall(client_socket, file_list, &out_len);
                    }
                    pclose(fp);
                }
                out_len = 4;
                // close(client_socket);
                sendall(client_socket, "\r\n\r\n", &out_len); // Send end of response
                break;
            }
            else
            {
                int out_len = 18;
                sendall(client_socket, "Unknown command\r\n\r\n", &out_len);
            }
        }
        else
        {
            put_file(buffer, &msg_size, file, n);
        }
        memset(buffer, 0, sizeof(buffer));
    }
}


int sendall(int s, char *buf, int *len)
{
    int total = 0;
    int bytesleft = *len;
    int n;

    while(total < *len)
    {
        if((n = send(s, buf+total, bytesleft, 0)) <= 0)
        {
            break;
        }
        else
        {
            total += n;
            bytesleft -= n;
        }
    }

    *len = total;

    return n==-1?-1:0;
}

char *get_hashed_filename(const char *path)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    char *hashed_path = malloc(33);

    EVP_MD_CTX * mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_get_digestbyname("MD5");

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, path, strlen(path));
    EVP_DigestFinal_ex(mdctx, digest, NULL);

    EVP_MD_CTX_free(mdctx);

    for (int i = 0; i < 16; i++) {
        sprintf(&hashed_path[i * 2], "%02x", (unsigned int)digest[i]);
    }

    return hashed_path;
}

int string_mod(const char *hashed_path, int modulo)
{
    size_t length = strlen(hashed_path);
    int sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += hashed_path[i];
    }
    return sum % modulo;
}


void free_and_close(int **sockets, int server_count)
{
    for (int i = 0; i < server_count; i++) {
        close((*sockets)[i]);
    }
    free(*sockets);
    *sockets = NULL;
}

void * thread_function()
{
    int *pclient;
    while(!flag)
    {
        pthread_mutex_lock(&mutex);
        if((pclient = dequeue()) == NULL)
        {
            pthread_cond_wait(&conditional, &mutex);
            if (!flag)
            {
                pclient = dequeue();
            }
        }
        pthread_mutex_unlock(&mutex);
        if(flag)
        {
            free(pclient);
            pthread_exit(NULL);
        }
        if(pclient != NULL)
        {
            connect_and_send(pclient);
            free(pclient);
        }
    }
    pthread_exit(NULL);
}


char ** read_server_file(int * num_servers) {
    FILE * file = fopen(strcat(getenv("HOME"), "/dfc.conf"), "r");
    if (file == NULL) {
        perror("Error opening CONF\n");
        return NULL;
    }

    char** servers = NULL;
    char line[100];
    int server_count = 0;
    int max_servers = 10; // Initial capacity for the array, can be adjusted as needed

    servers = malloc(max_servers * sizeof(char*));
    if (servers == NULL) {
        fclose(file);
        fprintf(stderr, "Error allocating memory\n");
        *num_servers = 0;
        return NULL;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // Remove newline character at the end of the line

        char *token = strtok(line, " "); // Split the line using space as a delimiter
        for (int i = 0; token != NULL; i++) {
            if (i == 2) {
                if (server_count >= max_servers) {
                    max_servers *= 2; // Double the capacity
                    char** tmp = realloc(servers, max_servers * sizeof(char*));
                    if (tmp == NULL) {
                        fclose(file);
                        free(servers);
                        fprintf(stderr, "Error reallocating memory\n");
                        *num_servers = 0;
                        return NULL;
                    }
                    servers = tmp;
                }

                servers[server_count] = str_dup(token);
                if (servers[server_count] == NULL) {
                    fclose(file);
                    free(servers);
                    fprintf(stderr, "Error duplicating string\n");
                    *num_servers = 0;
                    return NULL;
                }

                server_count++;
                break;
            }
            token = strtok(NULL, " ");
        }
    }

    fclose(file);
    *num_servers = server_count;
    return servers;
}

char *str_dup(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    char *new_str = (char *)malloc(len + 1);

    if (new_str == NULL) {
        return NULL;
    }

    memcpy(new_str, str, len);
    new_str[len] = '\0';

    return new_str;
}

DFSFile* processFile(FILE* file, int* numFiles) {
    DFSFile* dfsFiles = (DFSFile*) malloc(sizeof(DFSFile) * 1024);
    *numFiles = 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        int chunk_num;
        long timestamp, chunk_size;
        char filename[256];

        sscanf(line, "%d.%ld.%ld.%s", &chunk_num, &timestamp, &chunk_size, filename);

        // printf("chunk_num %d\n", chunk_num);


        int fileExists = 0;
        for (int i = 0; i < *numFiles; i++) {
            if (strcmp(dfsFiles[i].filename, filename) == 0) {
                fileExists = 1;
                if (chunk_num == 1)
                {
                    // printf("1\n");
                    dfsFiles[i].one = 1;
                }
                else if (chunk_num == 2)
                {
                    // printf("2\n");
                    dfsFiles[i].two = 1;
                }
                else if (chunk_num == 3)
                {
                    // printf("3\n");
                    dfsFiles[i].three = 1;
                }
                else if (chunk_num == 4)
                {
                    // printf("4\n");
                    dfsFiles[i].four = 1;
                }
                break;
            }
        }

        if (!fileExists) {
            DFSFile newFile;
            strcpy(newFile.filename, filename);
            newFile.one = (chunk_num == 1) ? 1 : 0;
            newFile.two = (chunk_num == 2) ? 1 : 0;
            newFile.three = (chunk_num == 3) ? 1 : 0;
            newFile.four = (chunk_num == 4) ? 1 : 0;
            dfsFiles[*numFiles] = newFile;
            (*numFiles)++;
        }
    }

    return dfsFiles;
}

void remove_special_characters(char* string) {
    char* source = string;
    char* destination = string;

    while (*source) {
        if (isalnum(*source) || *source == '.') {
            *destination = *source;
            destination++;
        }
        source++;
    }
    *destination = '\0';
}

File** file_discovery(int server_num, char** server_info, const char* filename, int * available) {
    File** files = (File**) malloc(sizeof(File*) * 4);

    // Initialize file pointers to NULL
    for (int i = 0; i < 4; i++) {
        files[i] = NULL;
    }

    // Iterate over all servers
    for (int i = 0; i < server_num; i++)
    {
        char buf[BUFSIZE + 1] = {0};
        int msg_size = 0;

        // Parse server address and port
        char server_address[50];
        int server_port;
        if (sscanf(server_info[i], "%49[^:]:%d", server_address, &server_port) != 2)
        {
            fprintf(stderr, "Invalid server address format\n");
            continue;
        }

        // Create socket
        int client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1)
        {
            perror("Error creating socket");
            continue;
        }

        // Set up server address
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_address, &(server_addr.sin_addr)) <= 0)
        {
            perror("Invalid server address");
            close(client_socket);
            continue;
        }

        // Connect to the server
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
        {
            //
            close(client_socket);
            continue;
        }

        // Send request to the server
        int p_sz = sprintf(buf, "discover %s\r\n\r\n", filename);
        if (sendall(client_socket, buf, &p_sz) == -1)
        {
            perror("Error sending request header to client\n");
            close(client_socket);
            continue;
        }

        // Listen for a response from the server
        while (1)
        {
            int n = read(client_socket, buf + msg_size, sizeof(buf) - msg_size - 1);
            if (n == -1)
            {
                perror("Error reading from socket");
                close(client_socket);
                break;
            }
            msg_size += n;

            if (msg_size > BUFSIZE || strstr(buf, "\r\n\r\n")) break;
        }

        // Parse the response and add files to the list
        char* carriage = strstr(buf, "\r\n\r\n");

        if (carriage)
        {
            char temp_buf[1025] = {0};
            strncpy(temp_buf, buf, carriage - buf);

            char* token = strtok(temp_buf, "\n");
            while (token != NULL)
            {
                token += 2;  // Skip "./"

                // Parse server number from token (assuming the format is "dfsX/...")
                int server_num = -1;
                if (sscanf(token, "dfs%d", &server_num) != 1)
                {
                    fprintf(stderr, "Invalid file path format\n");
                    token = strtok(NULL, "\n");
                    continue;
                }

                // Skip to the next part of the token
                token = strchr(token, '/') + 1;

                File* new_file = (File*) malloc(sizeof(File));
                new_file->sock_origin = server_num;
                new_file->nextfile = NULL;

                int chunk_num;
                char filename_buffer[250] = {0};
                sscanf(token, "%d.%ld.%ld.%s", &chunk_num, &new_file->timestamp, &new_file->chunk_size, filename_buffer);
                new_file->filename = strdup(filename_buffer);

                if (files[chunk_num - 1] == NULL)
                {
                    new_file->chunk_num = chunk_num;
                    files[chunk_num - 1] = new_file;
                    *available = *available + 1;
                } else
                {
                    new_file->chunk_num = chunk_num;
                    File* current = files[chunk_num - 1];
                    while (current->nextfile != NULL)
                    {
                        current = current->nextfile;
                    }
                    current->nextfile = new_file;
                }
                token = strtok(NULL, "\n");
            }
        }
        msg_size = 0;
    }
    return files;
}

void fetch_chunks(const char *filename, File **files, int files_count, char **hostinfo) {
    char buf[BUFSIZE];
    int file = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (file == -1) {
        perror("Failed to open file for writing");
        return;
    }

    for (int i = 0; i < files_count; i++) {
        File *chunk_new = files[i];
        File *chunk_num = files[i];
        long newest = 0;

        while (chunk_num->nextfile != NULL) {
            if (newest < chunk_num->timestamp) {
                newest = chunk_num->timestamp;
                chunk_new = chunk_num;
            }
            chunk_num = chunk_num->nextfile;
        }

        char server_address[16];
        int server_port;
        if (sscanf(hostinfo[chunk_new->sock_origin - 1], "%49[^:]:%d", server_address, &server_port) != 2) {
            fprintf(stderr, "Invalid server address format\n");
            continue;
        }

        int client_socket;
        struct sockaddr_in server_addr;

        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error creating socket");
            continue;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_address, &(server_addr.sin_addr)) <= 0) {
            perror("Invalid server address");
            close(client_socket);
            continue;
        }

        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            //
            close(client_socket);
            continue;
        }

        int p_sz = sprintf(buf, "get %d.%ld.%ld.%s\r\n\r\n", chunk_new->chunk_num, chunk_new->timestamp, chunk_new->chunk_size, chunk_new->filename);
        // printf("performing get of %s on %d\n", buf, chunk_new->sock_origin);

        check(sendall(client_socket, buf, &p_sz), "Error sending request header to client\n");

        uint16_t bytes_read = 0;
        int total_bytes_read = 0;
        char filebuff[BUFSIZE + 1];
        memset(filebuff, 0, sizeof(filebuff));

        while (total_bytes_read < chunk_new->chunk_size) {
            bytes_read = recv(client_socket, filebuff, sizeof(filebuff) - 1, 0);
            // printf("%s\n", filebuff);
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("The sender has closed the connection\n");
                } else {
                    perror("Error reading from socket");
                }
                break;
            }

            
            filebuff[bytes_read] = '\0'; // ensure null termination
            put_file(filebuff, &total_bytes_read, file, bytes_read);
            // write(file, filebuff, bytes_read);
            // total_bytes_read += bytes_read;
            memset(filebuff, 0, sizeof(filebuff));
        }

        close(client_socket);
    }

    close(file);
}



void list_files(char **hostinfo, int server_tot) {
    FILE *file = fopen("ls.txt", "wb");
    char buf[BUFSIZE];
    int n;

    for(int i = 0; i < server_tot; i++) {
        off_t msg_size = 0;
        memset(buf, 0, sizeof(buf));

        char server_address[16];
        int server_port;

        if (sscanf(hostinfo[i], "%49[^:]:%d", server_address, &server_port) != 2) {
            fprintf(stderr, "Invalid server address format\n");
            continue;
        }

        int client_socket;
        struct sockaddr_in server_addr;

        // Create socket
        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error creating socket");
            continue;
        }

        // Set up server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_address, &(server_addr.sin_addr)) <= 0) {
            perror("Invalid server address");
            close(client_socket);
            continue;
        }

        // Connect to the server
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            //
            close(client_socket);
            continue;
        }

        int p_sz = sprintf(buf,"ls ls\r\n\r\n");
        check(sendall(client_socket, buf, &p_sz), "Error sending request header to client\n");

        while(1) {
            if(check((n = recv(client_socket, buf+msg_size, sizeof(buf)-msg_size-1, 0)), "CONNECTION TIMEOUT")) {
                close(client_socket);
                continue;
            }
            msg_size += n;

            if(msg_size > BUFSIZE - 1 || strstr(buf, "\r\n\r\n")) break;
        }

        char * token;
        char * carriage;
        char temp_buf[1025];

        if((carriage = strstr(buf, "\r\n\r\n"))) {
            strncpy(temp_buf, buf, carriage - buf);
        }

        token = strtok(temp_buf, "\n");
        while (token != NULL) {
            remove_special_characters(token);
            fprintf(file, "%s\n", token);
            token = strtok(NULL, "\n");
        }
        msg_size = 0;
    }

    fclose(file);
    file = fopen("ls.txt", "rb");

    int numFiles;
    DFSFile *dfsFiles = processFile(file, &numFiles);

    for (int i = 0; i < numFiles; i++) {
        if (dfsFiles[i].one == 1 && dfsFiles[i].two == 1 &&
            dfsFiles[i].three == 1 && dfsFiles[i].four == 1) {
            printf("%s\n", dfsFiles[i].filename);
        } else {
            printf("%s is incomplete\n", dfsFiles[i].filename);
        }
    }
}

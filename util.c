#include "util.h"

volatile sig_atomic_t flag = 0;

char * cwd = NULL;

int connect_to_servers(int **sockets, int * server_tot)
{
    // FILE *file = fopen("dfc.conf", "r");
    FILE * file = fopen(strcat(getenv("HOME"), "/dfc.conf"), "r");
    if (file == NULL) {
        perror("Error opening file\n");
        return -1;
    }

    char line[256];
    char host[50];
    int port;

    struct sockaddr_in server_addr;
    int socket_fd;
    int server_count = 0;

    while (fgets(line, sizeof(line), file)) {
        printf("CONNECTED SERVER\n");
        *server_tot = *server_tot + 1;
        if (sscanf(line, "server %*s %49[^:]:%d", host, &port) == 2) {
            socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd < 0) {
                perror("Error creating socket");
                printf("Error creating socket\n");
                fclose(file);
                return -1;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
                perror("Error converting IP address");
                printf("Error converting IP address\n");
                close(socket_fd);
                fclose(file);
                return -1;
            }

            if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Error connecting to server");
                printf("Error connecting to server\n");
                close(socket_fd);
                continue;
            }

            *sockets = realloc(*sockets, (server_count + 1) * sizeof(int));
            (*sockets)[server_count++] = socket_fd;
        }
    }
    printf("END OF GETTING SERVERS\n");
    fclose(file);
    return server_count;
}

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

    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout));
    for (;;) {
        memset(buffer, 0, BUFSIZE + 1);
        msg_size = 0;
        buffer[BUFSIZE] = '\0';

        while (1)
        {
            if (check((n = read(client_socket, buffer + msg_size, sizeof(buffer) - msg_size - 1)), "CONNECTION TIMEOUT"))
            {
                close(client_socket);
                return;
            }
            msg_size += n;
            if (msg_size > BUFSIZE - 1 || strstr(buffer, "\r\n\r\n")) break;
        }
        printf("BUFFER %s\n", buffer);

        // add handling for multiple filenames and also check for less than 2 assignments
        sscanf(buffer, "%s %s", command, full_filename);

        // Handle "discover" command
        if (strcmp(command, "discover") == 0) {
            // Use popen() to get the file list and send it back to the client
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
            sendall(client_socket, "\r\n\r\n", &out_len); // Send end of response
        }
        else if (strcmp(command, "get") == 0)
        {
            // Send the requested file to the client
            off_t offset = 0;
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, full_filename);
            int filefd = open(full_path, O_RDONLY);
            if (filefd != -1)
            {
                struct stat st;
                if (fstat(filefd, &st) == 0)
                {
                    off_t file_size = st.st_size;
                    while (file_size > 0)
                    {
                        ssize_t sent = sendfile(client_socket, filefd, &offset, file_size);
                        if (sent == -1)
                        {
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
            }
        }
        else if (strcmp(command, "put") == 0)
        {
            memset(buffer, 0, 1025);
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, full_filename);
            int file = open(full_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (file != -1)
            {
                while ((n = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0)
                {
                    write(file, buffer, n);
                }
                close(file);
            }
            else
            {
                perror("Error creating file");
            }
        }
        else if (strcmp(command, "ls") == 0)
        {
            char output[BUFSIZE];
            int out_len = 4;
            char cmd[BUFSIZ];
            sprintf(cmd, "ls %s", cwd);
            FILE *fp = popen(cmd, "r");
            if (fp != NULL) {
                while (fgets(output, sizeof(output) - 1, fp) != NULL) {
                    out_len = strlen(output);
                    sendall(client_socket, output, &out_len);
                }
                pclose(fp);
            }
            out_len = 4;
            sendall(client_socket, "\r\n\r\n", &out_len); // Send end of response
        }
        else
        {
            int out_len = 18;
            sendall(client_socket, "Unknown command\r\n\r\n", &out_len);
        }
    }
    close(client_socket);
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
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/stat.h>
#include "util.h"


void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd;
    char buf[BUFSIZE];
    struct addrinfo hints, *servinfo, *p;
    size_t n;
    int status;
    char cmd[10];
    char filename[256];
    int server_num;
    int server_tot = 0;
    // handling n servers
    int *socks = NULL;

    char * config = read_file_into_buffer();

    // downloading or putting by opening a new file to write to
    int fp;

    // change back to 3
    if (argc != 3)
    {
       fprintf(stderr,"usage: %s <hostname> <file(s)>\n", argv[0]);
       exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      return 1;
    }


    for (p = servinfo; p != NULL; p = p->ai_next)
    {
      if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
      {
        perror("error making socket\n");
        continue;
      }
      break;
    }

    if (p == NULL)
    {
      perror("failed to create to socket\n");
      return 2;
    }

    while (strcmp(cmd, "exit") != 0)
    {
        sleep(1);
        printf("AFTER SLEEP\n");
        server_tot = 0;
        server_num = connect_to_servers(&socks, &server_tot, config);
        printf("server_tot: %d\n", server_tot);
        memset(buf, 0, sizeof(buf));
        printf("Please enter msg: ");
        fgets(buf, sizeof(buf)-1, stdin);
        sscanf(buf, "%s %s", cmd, filename);

        if(!strcmp(cmd,"get"))
        {
        int available = 0;
        off_t msg_size = 0;
        File **files = (File **)calloc(server_tot, sizeof(File *));

        for (int i = 0; i < server_num; i++)
        {
            memset(buf, 0, sizeof(buf));
            int p_sz = sprintf(buf,"discover %s\r\n\r\n", filename);
            check(sendall(socks[i], buf, &p_sz), "Error sending request header to client\n");

            while(1)
            {
                if(check((n = read(socks[i], buf+msg_size, sizeof(buf)-msg_size-1)), "CONNECTION TIMEOUT"))
                {
                    close(socks[i]);
                    continue; // if server shuts down before response can be sent we'll just continue getting the other filenames
                }
                msg_size += n;

                if(msg_size > BUFSIZE - 1 || strstr(buf, "\r\n\r\n")) break;
            }

            // printf("%s\n", buf);

            char * carriage;
            char temp_buf[1025];
            if((carriage = strstr(buf, "\r\n\r\n")))
            {
                strncpy(temp_buf, buf, carriage - buf);
            }

            char *token;
            token = strtok(temp_buf, "\n");
            while (token != NULL)
            {
                token += 7;
                printf("token: %s\n", token);
                File *new_file = (File *)malloc(sizeof(File));
                new_file->sock_origin = socks[i];
                new_file->nextfile = NULL;

                int chunk_num;
                char filename_buffer[250];
                filename_buffer[250] = '\0';
                sscanf(token, "%d.%ld.%ld.%s", &chunk_num, &new_file->timestamp, &new_file->chunk_size, filename_buffer);

                new_file->filename = str_dup(filename_buffer);
                printf("filename: %s\n", new_file->filename);


                printf("chunk_num: %d\n", chunk_num);
                if (files[chunk_num-1] == NULL)
                {
                    new_file->chunk_num = chunk_num;
                    files[new_file->chunk_num - 1] = new_file;
                    available++;
                }
                else
                {
                    new_file->chunk_num = chunk_num;
                    File * current = files[chunk_num - 1];
                    while (current->nextfile != NULL)
                    {
                        current = current->nextfile;
                    }
                    current->nextfile = new_file;
                }

                token = strtok(NULL, "\n");
            }
            msg_size = 0;
        }

        // printf("available : %d, total_chunks : %d\n", available, server_tot);
        if(server_tot != available)
        {
            printf("%s is incomplete\n", filename);
            break;
        }

        // now that we have all the chunks we can just start requesting them and writing them to disk
        FILE *file = fopen(filename, "wb");

        for(int i = 0; i < server_tot; i++)
        {
            File * chunk_new = files[i];
            File * chunk_num = files[i];
            long newest = 0;

            while(chunk_num->nextfile != NULL)
            {
                if(newest < chunk_num->timestamp)
                {
                    newest = chunk_num->timestamp;
                    chunk_new = chunk_num;
                }
                chunk_num = chunk_num->nextfile;
            }
            int p_sz = sprintf(buf,"get %d.%ld.%ld.%s\r\n\r\n", chunk_new->chunk_num, chunk_num->timestamp, chunk_num->chunk_size, chunk_num->filename);
            printf("performing get of %s on %d\n", buf, chunk_new->sock_origin);
            check(sendall(chunk_new->sock_origin, buf, &p_sz), "Error sending request header to client\n");

            // receiving file and writing the chunks to single file
            uint16_t bytes_read = 0;
            uint16_t total_bytes_read = 0;
            memset(buf, 0, sizeof(buf));
            while (total_bytes_read < chunk_new->chunk_size)
            {
                bytes_read = read(chunk_new->sock_origin, buf, sizeof(buf) - 1);
                buf[BUFSIZE] = '\0';
                if (bytes_read <= 0) {
                    perror("Error reading from sock_origin");
                    break;
                }
                printf("write buffer: %s\n", buf);
                fwrite(buf, 1, bytes_read, file);
                total_bytes_read += bytes_read;
            }

        }
        free_and_close(&socks, server_num);
        fclose(file);
        continue;
      }
      else if (!strcmp(cmd, "put"))
      {
        if(server_tot < 4)
        {
            printf("%s put failed\n", filename);
        }
        struct stat st;
        time_t curr_time;
        long size_chunk;
        long last_chunk;
        long curr_chunk;
        off_t start_one = 0;
        off_t start_two = 0;
        char * hash;
        int mod;
        time(&curr_time);

        if(!stat(filename, &st))
        {
            size_chunk = st.st_size / server_num;
            last_chunk = st.st_size - (size_chunk * (server_num - 1)); 
            hash = get_hashed_filename(filename);
            mod = string_mod(hash, server_num);
            fp = open(filename, O_RDONLY);
        }
        else
        {
            return 1;
        }

        curr_chunk = size_chunk;
        int p_sz;
        // First instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 1, curr_time, curr_chunk, filename);
        check(sendall(socks[(-mod + 0) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 0) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 0) % server_num], fp, &start_one, curr_chunk);
        // Second instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 2, curr_time, curr_chunk, filename);
        check(sendall(socks[(-mod + 1) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 1) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 1) % server_num], fp, &start_one, curr_chunk);

        // Third instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 3, curr_time, curr_chunk, filename);
        check(sendall(socks[(-mod + 2) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 2) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 2) % server_num], fp, &start_one, curr_chunk);

        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 4, curr_time, last_chunk, filename);
        check(sendall(socks[(-mod + 3) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 3) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 3) % server_num], fp, &start_one, last_chunk);


        free_and_close(&socks, server_tot);
        server_tot = 0;
        connect_to_servers(&socks, &server_tot, config);
        close(fp);
        fp = open(filename, O_RDONLY);

        // SECOND CHUNKS
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 1, curr_time, curr_chunk, filename);
        printf("%d\n", (-mod + 1) % server_num);
        check(sendall(socks[(-mod + 1) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 1) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 1) % server_num], fp, &start_two, curr_chunk);

        // Second instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 2, curr_time, curr_chunk, filename);
        check(sendall(socks[(-mod + 2) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 2) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 2) % server_num], fp, &start_two, curr_chunk);

        // Third instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 3, curr_time, curr_chunk, filename);
        check(sendall(socks[(-mod + 3) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 3) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 3) % server_num], fp, &start_two, curr_chunk);

        // Fourth instance
        memset(buf, 0, sizeof(buf));
        p_sz = sprintf(buf, "put %d.%ld.%ld.%s\r\n\r\n", 4, curr_time, last_chunk, filename);
        check(sendall(socks[(-mod + 4) % server_num], buf, &p_sz), "Error sending request header to client\n");
        recv(socks[(-mod + 4) % server_num], buf, sizeof(buf), 0);
        sendfile(socks[(-mod + 4) % server_num], fp, &start_two, last_chunk);

        free_and_close(&socks, server_tot);
        close(fp);
        continue;
      }
    }
    return 0;
}
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

    printf("GOT TO LOOP\n");
    // ACTION : CALL FUNCTION TO READ THROUGH FILE AND DYNAMICALLY ALLOCATE SPACE FOR NEW SOCKETS READ IN

    while (strcmp(cmd, "exit") != 0)
    {
        sleep(1);
        printf("AFTER SLEEP\n");
        server_num = connect_to_servers(&socks, &server_tot);
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

            char *token;
            token = strtok(buf, " ");
            while (token != NULL)
            {
                File *new_file = (File *)malloc(sizeof(File));
                new_file->sock_origin = socks[i];
                new_file->nextfile = NULL;

                int chunk_num;
                char filename_buffer[250];
                sscanf(token, "%d.%ld.%ld.%s", &chunk_num, &new_file->timestamp, &new_file->chunk_size, filename_buffer);
                new_file->filename = strdup(filename_buffer);


                if (files[chunk_num] == NULL)
                {
                    new_file->chunk_num = chunk_num;
                    files[chunk_num] = new_file;
                    available++;
                }
                else
                {
                    new_file->chunk_num = chunk_num;
                    File * current = files[chunk_num];
                    while (current->nextfile != NULL)
                    {
                        current = current->nextfile;
                    }
                    current->nextfile = new_file;
                }

                token = strtok(NULL, " ");
            }
            msg_size = 0;
        }

        if(server_tot != available)
        {
            break;
        }

        // now that we have all the chunks we can just start requesting them and writing them to disk
        FILE *file = fopen(filename, "wb");

        for(int i = 0; i++ < server_tot;)
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
            check(sendall(chunk_new->sock_origin, buf, &p_sz), "Error sending request header to client\n");

            // receiving file and writing the chunks to single file
            uint16_t bytes_read = 0;
            uint16_t total_bytes_read = 0;
            while (total_bytes_read < chunk_new->chunk_size) {
                bytes_read = read(chunk_new->sock_origin, buf, sizeof(buf));
                if (bytes_read <= 0) {
                    perror("Error reading from sock_origin");
                    break;
                }
                fwrite(buf, 1, bytes_read, file);
                total_bytes_read += bytes_read;
            }
            fclose(file);
        }
        free_and_close(&socks, server_num);
        fclose(file);
      }
      else if (!strcmp(cmd, "put"))
      {
        struct stat st;
        time_t curr_time;
        long size_chunk;
        long last_chunk;
        long curr_chunk;
        off_t start_one = 0;
        off_t start_two = 0;
        char * hash;
        int mod;

        memset(buf, 0, sizeof(buf));
        time(&curr_time);

        if(!stat(filename, &st))
        {
            size_chunk = st.st_size / server_num;
            last_chunk = st.st_size - (size_chunk * server_num - 1); 
            hash = get_hashed_filename(filename);
            mod = string_mod(hash, server_num);
            fp = open(filename, O_RDONLY);
        }

        for(int i = 0; i < server_num; i++)
        {
            curr_chunk = (i == server_num - 1) ? last_chunk : size_chunk;
            int p_sz = sprintf(buf,"put %d.%ld.%ld.%s\r\n\r\n", i, curr_time, curr_chunk, filename);
            // printf("Sending to : %d of size %d\n", socks[(-mod + i) % server_num], p_sz);
            // printf("Sending to : %d of size %d\n", socks[(-mod + i + 1) % server_num], p_sz);
            check(sendall(socks[(-mod + i) % server_num], buf, &p_sz), "Error sending request header to client\n");
            check(sendall(socks[(-mod + i + 1) % server_num], buf, &p_sz), "Error sending request header to client\n");
            sendfile(socks[(-mod + i) % server_num], fp, &start_one, curr_chunk);
            sendfile(socks[(-mod + i + 1) % server_num], fp, &start_two, curr_chunk);
        }
        free_and_close(&socks, server_num);
        close(fp);
      }
    }
    return 0;
}
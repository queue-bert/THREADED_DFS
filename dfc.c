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
    // size_t n;
    int status;
    // char cmd[10];
    // char filename[256];
    int server_num;
    int server_tot = 0;
    // handling n servers
    int *socks = NULL;
    int num_server_lines;

    char ** hostinfo = read_server_file(&num_server_lines);

    if (argc < 2)
    {
       fprintf(stderr,"usage: %s <cmd> <file(s)>\n", argv[0]);
       exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if((status = getaddrinfo("localhost", "8080", &hints, &servinfo)) != 0)
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

    int index = 1;
    char * cmd = argv[1];

    while (index < argc)
    {
        
        if(strcmp(cmd, "ls"))
        {
            index++;
        }
        if(index == argc) break;
        // printf("index: %d, argc: %d\n", index, argc);
        sleep(1);
        server_tot = 0;
        server_num = connects_to_servers(&socks, &server_tot, hostinfo, num_server_lines);
        memset(buf, 0, sizeof(buf));
        char * filename = argv[index];

        if(!strcmp(cmd,"get"))
        {
            int available = 0;

            File **files = file_discovery(num_server_lines, hostinfo, filename, &available);

            for(int i = 0; i < available; i++)
            {
                File *current = files[i];
                while (current != NULL)
                {
                    // printf("File name: %d.%ld.%ld.%s\n", current->chunk_num, current->timestamp, current->chunk_size, current->filename);
                    current = current->nextfile;
                }
            }

            if(num_server_lines != available)
            {
                printf("%s is incomplete\n", filename);
                continue;
            }

            fetch_chunks(filename, files, available, hostinfo);
            // index++;
        }
        else if (!strcmp(cmd, "put"))
        {

        if(server_tot < 4)
        {
            printf("%s put failed\n", filename);
        }
        struct stat st;
        long size_chunk;
        long last_chunk;
        off_t start_one = 0;
        off_t start_two = 0;
        char * hash;
        int mod;
        int fp1;
        int fp2;

        if(!stat(filename, &st))
        {
            size_chunk = st.st_size / server_num;
            last_chunk = st.st_size - (size_chunk * (server_num - 1)); 
            hash = get_hashed_filename(filename);
            mod = string_mod(hash, server_num);
            fp1 = open(filename, O_RDONLY);
            fp2 = open(filename, O_RDONLY);
        }
        else
        {
            return 1;
        }

        for(int i = 0; i < server_tot; i++)
        {
            int curr_chunk = (i < server_tot - 1) ? size_chunk : last_chunk;
            int first_chunk = (mod + i) % 4;
            int second_chunk = (first_chunk + 1) % 4;
            send_file_to_server(hostinfo[first_chunk], filename, fp1, &start_one, curr_chunk, i + 1);
            send_file_to_server(hostinfo[second_chunk], filename, fp2, &start_two, curr_chunk, i + 1);
        }
        // index++;
      }
      else if(!strcmp(cmd,"ls"))
      {
        printf("AVAILABLE FILES:\n");
        list_files(hostinfo, num_server_lines);
        index++;
      }
    }
    return 0;
}
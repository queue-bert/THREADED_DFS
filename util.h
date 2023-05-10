#ifndef UTIL_H
#define UTIL_H
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
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include "queue.h"
#include <signal.h>

#define BACKLOG 64
#define POOL_THREADS 10

typedef struct File {
    char * filename;
    int chunk_num;
    long timestamp;
    long chunk_size;
    int sock_origin;
    struct File * nextfile;
} File;

extern volatile sig_atomic_t flag;

extern char * cwd;

int connect_to_servers(int **sockets, int * server_tot);

char *get_hashed_filename(const char *path);

int string_mod(const char *hashed_path, int modulo);

void free_and_close(int **sockets, int server_count);

int check(int stat, char* message);

int sendall(int s, char *buf, int *len);

void * thread_function();

#endif
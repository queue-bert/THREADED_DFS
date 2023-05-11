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
#include <ctype.h>

#define BACKLOG 64
#define POOL_THREADS 10

typedef struct File {
    char * filename;
    int chunk_num;
    long timestamp; // time_t
    long chunk_size; // off_t
    int sock_origin;
    struct File * nextfile;
} File;

typedef struct DFSFile {
    char filename[256];
    int one;
    int two;
    int three;
    int four;
} DFSFile;

DFSFile * processFile(FILE* file, int* numFiles);

extern volatile sig_atomic_t flag;

extern char * cwd;

extern char * config;

int connect_to_servers(int **sockets, int *server_tot, const char *buffer);

char *get_hashed_filename(const char *path);

int string_mod(const char *hashed_path, int modulo);

void free_and_close(int **sockets, int server_count);

int check(int stat, char* message);

int sendall(int s, char *buf, int *len);

char *read_file_into_buffer();

void * thread_function();

char *str_dup(const char *str);

void remove_special_characters(char* string);

#endif
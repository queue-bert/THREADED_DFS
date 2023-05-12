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
#include <limits.h>

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

char *get_hashed_filename(const char *path);

int string_mod(const char *hashed_path, int modulo);

void free_and_close(int **sockets, int server_count);

int check(int stat, char* message);

int sendall(int s, char *buf, int *len);


void * thread_function();

char *str_dup(const char *str);

void remove_special_characters(char* string);
int connects_to_servers(int** sockets, int* server_tot, char** host_info, int num_servers);

char** read_server_file(int* num_servers);

File** file_discovery(int server_num, char** server_info, const char* filename, int * available);


// void put_file(char *buffer, int *bytes_written, int file_descriptor);
void put_file(char *buffer, int *bytes_written, int file_descriptor, int bytes);

void send_file_to_server(char* server_info, const char * filename, int fp, long * start, long curr_chunk, int chunk);

// void send_file_to_server(char ** server_info, const char * filename, int fp, long * start, long curr_chunk, int chunk, int mod, int servernum);

void fetch_chunks(const char *filename, File **files, int files_count, char **hostinfo);

void list_files(char **hostinfo, int server_tot);

#endif
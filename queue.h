#ifndef QUEUE_H
#define QUEUE_H

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>

#define QUEUE_SIZE 100
#define BUFSIZE 1024

extern pthread_mutex_t mutex;
extern pthread_cond_t conditional;

void enqueue(int* item);
int * dequeue();
int * peek();

#endif

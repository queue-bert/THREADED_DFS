#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include <netinet/in.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t conditional = PTHREAD_COND_INITIALIZER;


int* queue[QUEUE_SIZE];
int queue_counter = 0;

void enqueue(int* item) {
  if (queue_counter == QUEUE_SIZE) {
    printf("Queue is full\n");
    return;
  } else {
    queue[queue_counter] = item;
    queue_counter++;
  }
}

int* dequeue() {
  if (queue_counter == 0) {
    printf("Queue is empty\n");
    return NULL;
  } else {
    int* item = queue[0];
    for (int i = 0; i < queue_counter - 1; i++) {
      queue[i] = queue[i + 1];
    }
    queue_counter--;
    return item;
  }
}

int* peek() {
  if (queue_counter == 0) {
    printf("Queue is empty\n");
    return NULL;
  } else {
    return queue[0];
  }
}

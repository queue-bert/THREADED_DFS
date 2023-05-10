#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "util.h"

void sigint_handler(int sig)
{
  printf("Received the signal %d\n", sig);
  flag = 1;
  pthread_cond_broadcast(&conditional);
}

int main(int argc, char **argv) {
  int sockfd, new_connect;
  socklen_t clientlen;
  struct sockaddr_storage their_addr;
  int optval = 1;
  struct addrinfo hints, *res, *p;
  int status;
  pthread_t thread_pool[POOL_THREADS];

  // Registering signal handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa, NULL);

  for(int i = 0; i < POOL_THREADS; i++)
  {
    pthread_create(&thread_pool[i], NULL, thread_function, (void*)thread_pool);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;


  if (argc != 3) {
    fprintf(stderr, "usage: %s <dir> <port_num>\n", argv[0]);
    exit(1);
  }

  if((status = getaddrinfo(NULL, argv[2], &hints, &res)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 1;
  }

  cwd = argv[1];
  
  for (p = res; p != NULL; p = p->ai_next)
  {
    if(check((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)), "listener: socket")) continue;
    if(check(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)), "Set socket error")) continue;
    if(check(bind(sockfd, p->ai_addr, p->ai_addrlen), "listener: bind() error")) exit(1);
    if(check(listen(sockfd, BACKLOG), "listener: listen() error")) exit(1);
    break;
  }
  freeaddrinfo(res);

  
  for (;;)
  {
    if (flag == 1)
      break;
    clientlen = sizeof their_addr;
    if(check((new_connect = accept(sockfd, (struct sockaddr *)&their_addr, &clientlen)), "Couldn't accept connection!")) continue; // accept() is blocking
    int *pclient = malloc(sizeof(int));
    *pclient = new_connect;
    pthread_mutex_lock(&mutex);
    enqueue(pclient);
    pthread_cond_signal(&conditional);
    pthread_mutex_unlock(&mutex);
  }

  for(int i = 0; i < POOL_THREADS; i++)
  {
    pthread_join(thread_pool[i], NULL);
  }

  close(sockfd);

  return 0;
}

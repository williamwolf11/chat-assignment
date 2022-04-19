/*
 * chat-client.c
 * Authors: Will Wolf, Kevin Ewing
 * Initial code copied then modified from Pete's echo-client.c
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define BUF_SIZE 4096

void *read_terminal(void *data);

/* struct to hold our file descriptors */

int current_fd;
pthread_t thread;
int terminal_bytes_received;
int server_bytes_received;
char buf[BUF_SIZE];

int main(int argc, char *argv[])
{
   char *dest_hostname, *dest_port;
   struct addrinfo hints, *res;
   int rc;
   int conn_fd;

   dest_hostname = argv[1];
   dest_port     = argv[2];

   /* create a socket */
   if((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
      perror("socket");
      exit(1);
   }

   /* client usually doesn't bind, which lets kernel pick a port number */

   /* but we do need to find the IP address of the server */
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
      printf("getaddrinfo failed: %s\n", gai_strerror(rc));
      exit(1);
   }

   /* connect to the server */
   if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
      perror("connect");
      exit(2);
   }

   printf("Connected\n");
   /* assign socket to our variable */
   current_fd = conn_fd;

   /* Create a thread for reading from terminal */
   pthread_create(&thread, NULL, read_terminal, &current_fd);

   /* continuously read input from server and print it out formatted with a time stamp */
   while((server_bytes_received = recv(conn_fd, buf, BUF_SIZE, 0)) > 0) {
      time_t current_time = time(NULL);
      struct tm *ptm = localtime(&current_time);
      printf("%02d:%02d:%02d: %s", ptm->tm_hour, ptm->tm_min, ptm->tm_sec, buf);
      memset(buf, 0, sizeof(buf));
   }
   /* if the server shuts down, alert users */
   if(server_bytes_received == 0){
      printf("Connection closed by remote host.\n");
   }
   else{
      perror("recv");
   }
   if(close(current_fd) < 0){
      perror("close");
   }
   exit(0);
   return 0;
}


void *read_terminal(void *data){
   /* cast parameter to int type instead of void pointer */
   int *fd = (int *) data;
   /* continue reading input from terminal and sending it to server until client disconnects or presses ctrl-d */
   while((terminal_bytes_received = read(0, buf, BUF_SIZE)) > 0 && !feof(stdin)) {
      fflush(stdout);
      if(send(*fd, buf, terminal_bytes_received, 0) < 0){
         perror("send");
      }
      memset(buf, 0, sizeof(buf));
   }
   printf("Exiting.\n");
   exit(0);
   return NULL;
}

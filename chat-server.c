/*
 * chat-server.c
 * Authors: Will Wolf, Kevin Ewing
 * Initial code copied then modified from Pete's echo-server.c
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define BACKLOG 10
#define BUF_SIZE 4096

/*function prototypes*/
void *input_handler(void *data);

/* struct which will be used to create a linked list of client socket file descriptors */
typedef struct fd_list_elt{
   int current_fd;
   struct fd_list_elt * next;
   struct fd_list_elt * prev;
   pthread_t thread;
} fd_list_elt;

char *listen_port;
char *remote_ip;
uint16_t remote_port;
int listen_fd, conn_fd;
struct sockaddr_in remote_sa;
int bytes_received;
fd_list_elt *head;
pthread_mutex_t mutex;

int main(int argc, char *argv[])
{
   /* start things off by accepting incoming connections */
   listen_port = argv[1];
   struct addrinfo hints, *res;
   int rc;
   socklen_t addrlen;

   /* create a socket */
   if((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
      perror("socket");
      exit(1);
   }

   /* bind it to a port */
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;
   if((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
      printf("getaddrinfo failed: %s\n", gai_strerror(rc));
      exit(1);
   }

   if(bind(listen_fd, res->ai_addr, res->ai_addrlen) < 0){
      perror("bind");
      exit(1);
   }

   /* start listening */
   if(listen(listen_fd, BACKLOG) < 0){
      perror("listen");
      exit(1);
   }

   /* infinite loop of accepting new connections and handling them */
   while(1) {

         /* accept a new connection*/
         addrlen = sizeof(remote_sa);
         if((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) < 0) {
           perror("accept");
           exit(1);
         }

         /* lock the linked list while we update it */
         pthread_mutex_lock(&mutex);

         /* create a new element of the linked list (for the new connection) and insert it into the list properly */
         fd_list_elt *stored_fd = malloc(sizeof(fd_list_elt));
         if(stored_fd == NULL){
            fprintf(stderr, "Fatal: failed to allocate %zu bytes.\n", sizeof(fd_list_elt));
            exit(1);
         }

         fd_list_elt *temp_fd;
         stored_fd->current_fd = conn_fd;
         if(head == NULL){
            head = stored_fd;
            stored_fd->next = head;
            stored_fd->prev = head;
         }
         else{
            temp_fd = head;
            while(temp_fd->next != head){
               temp_fd = temp_fd->next;
            }
            temp_fd->next = stored_fd;
            stored_fd->prev = temp_fd;
            head->prev = stored_fd;
            stored_fd->next = head;
            head = stored_fd;
         }

         /* unlock the list because we are done manipulating it */
         pthread_mutex_unlock(&mutex);

         /* spin off a new thread for this socket to handle inputs */
         pthread_create(&stored_fd->thread, NULL, input_handler, stored_fd);
   }
   return 0;
}

void *
input_handler(void *data){
   char nickname[30] = "unknown";
   char new_nickname[30];
   char buf[BUF_SIZE];
   fd_list_elt *fd = (fd_list_elt *) data;

   /* announce our communication partner */
   remote_ip = inet_ntoa(remote_sa.sin_addr);
   remote_port = ntohs(remote_sa.sin_port);
   printf("new connection from %s:%d\n", remote_ip, remote_port);

   /* receive input strings and handle them appropriately until the other end closes the connection */
   while((bytes_received = recv(fd->current_fd, buf, BUF_SIZE, 0)) > 0) {
      fflush(stdout);
      char new_buf[BUF_SIZE + 31];
      /* if the input begins with /nick, change the user's nickname */
      if(strncmp(buf, "/nick ", 6) == 0){
         snprintf(new_nickname, strlen(&buf[6]), "%s", &buf[6]);
         printf("User %s (%s:%d) is now known as %s.\n", nickname, remote_ip, remote_port, new_nickname);
         snprintf(new_buf, BUF_SIZE + 31, "User %s is now known as %s.\n", nickname, new_nickname);
         snprintf(nickname, 30, "%s", new_nickname);
      }
      /*otherwise, we're dealing with a standard chat message -- send it out to all of our clients formatted with the nickname at the front */
      else{
         snprintf(new_buf, BUF_SIZE + 31, "%s: %s", nickname, buf);
      }
      if(send(fd->current_fd, new_buf, strlen(new_buf) + 1, 0) < 0){
         perror("send");
         exit(1);
      }

      /* lock the linked list while we iterate through it */
      pthread_mutex_lock(&mutex);

      fd_list_elt *x = (fd_list_elt *)fd->next;
      while(x != fd){
        if(send(x->current_fd, new_buf, strlen(new_buf) + 1, 0) < 0){
           perror("send");
           exit(1);
        }
         x = (fd_list_elt *) x -> next;
      }

      /* unlock the list because we are done iterating through it */
      pthread_mutex_unlock(&mutex);

      memset(buf, 0, sizeof(buf));
   }
   printf("Lost connection from %s.\n", nickname);
   /*when a client disconnects alert the other clients and remove their struct from the linked list*/
   char new_buf2[BUF_SIZE];
   int string_size = snprintf(new_buf2, BUF_SIZE, "User %s has disconnected.\n", nickname);

   fd_list_elt *x = (fd_list_elt *)fd->next;
   while(x != fd){
      if(send(x->current_fd, new_buf2, string_size, 0) < 0){
         perror("send");
         exit(1);
      }
      x = (fd_list_elt *) x -> next;
   }
   if(close(fd->current_fd) < 0){
      perror("close");
      exit(1);
   }
   /* lock the linked list while we update it */
   pthread_mutex_lock(&mutex);

   fd->prev->next = fd->next;
   fd->next->prev = fd->prev;
   if(fd == head){
      head = fd->prev;
   }

   /* unlock the list because we are done manipulating it */
   pthread_mutex_unlock(&mutex);

   free(fd);
   return NULL;
}

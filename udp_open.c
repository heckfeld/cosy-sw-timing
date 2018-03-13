
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

extern int errno;

int
create_recv_socket(int port, struct sockaddr_in *myaddr_in)
{
   int s;

   myaddr_in->sin_family = AF_INET;
   myaddr_in->sin_addr.s_addr = INADDR_ANY;
   myaddr_in->sin_port = port;

   s = socket (AF_INET, SOCK_DGRAM, 0);
   if (s == -1) {
      printf("unable to create socket\n");
      exit(1);
   }
      /* Bind the server's address to the socket. */
   if (bind(s, (struct sockaddr*)myaddr_in, sizeof(struct sockaddr_in)) == -1)
   {
      printf("unable to bind address\n");
      exit(1);
   }
   return (s);
}

int
create_send_socket(clientaddr_in)
struct sockaddr_in *clientaddr_in;
{
   int s;

   memset ((char *)clientaddr_in, 0, sizeof(struct sockaddr_in));
   clientaddr_in->sin_family = AF_INET;
   clientaddr_in->sin_addr.s_addr = INADDR_ANY;
   clientaddr_in->sin_port = 0;

/********
   memset ((char *)addr_out, 0, sizeof(struct  sockaddr_in));
   addr_out->sin_family = AF_INET;
   addr_out->sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   addr_out->sin_port = port;
*********/

   s = socket (AF_INET, SOCK_DGRAM, 0);
   if (s == -1) {
      printf("unable to create socket\n");
      exit(1);
   }
      /* Bind the server's address to the socket. */
   if (bind(s, (struct sockaddr *)clientaddr_in, 
           sizeof(struct  sockaddr_in)) == -1)
   {
      printf("unable to bind address\n");
      exit(1);
   }
/*******
   addrlen = sizeof(struct  sockaddr_in);
******/
   return (s);
}

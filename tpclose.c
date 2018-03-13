#include <server.h>

#define MAX_SOCKETS 128

extern struct gui guis[MAX_SOCKETS * 2];

extern int tcpmask[MAX_SOCKETS];
extern struct fd_set readmask, readfds;

int tpclose(k)
int k;
{
   close(k);
   guis[k].ptr = 0;
   guis[k].socket = 0;
   guis[k].port = 0;
   guis[k].name[0] = '\0';
   guis[k].inet_addr = 0;
   guis[k].ptr = 0;
   readmask.fds_bits[k/NFDBITS] &= ~tcpmask[k];
   tcpmask[k] = 0;
}

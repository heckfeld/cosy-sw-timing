#define MAXMSG 81920

struct cctpmsg {
        long    mtype;
        char  mtext [MAXMSG];
};

EXTERN struct cctpmsg *msgs;

# include <stdio.h>
# include <stdlib.h>             /* fuer 'malloc()', 'exit()' */
# include <string.h>             /* fuer 'strlen()', 'memset()' */

# include <sys/socket.h>
# include <netinet/in.h>
#include <netdb.h>
# include <arpa/inet.h>

# include <unistd.h>             /* fuer 'getpid()', 'fork()' */
# include <sys/msg.h>            /* fuer 'msgrcv()', msgsnd()' */

# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/ipc.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

# include <signal.h>

# include <errno.h>
extern int errno;

#define MAX_SOCKETS 128
#define MASK(f)     (1 << (f))
#define NWORDS  howmany(MAX_SOCKETS, NFDBITS)
typedef struct gui_tab {
   int addr_port;
   int sw_timing_port;
   int ust_port;
   int reception_socket;
   int active_socket;
   int not_alive;
   unsigned long inet_addr;
   char name[64];
   char host[64];
   char display[64];
   char title[64];
   int is_listed;
   struct gui_tab *next;
   struct gui_tab *prev;
} GUITAB;
EXTERN GUITAB guitab[MAX_SOCKETS + 1], tmp_guitab; 
EXTERN GUITAB *free_guitab_head, *guitab_head;

#define MAX_TRIGGER 2048
typedef struct trigger *TRIGGER_PTR;
typedef struct trigger
{
   unsigned int    time;
   struct gui_tab  *guitab_ptr;
   char            cmd[256];
   TRIGGER_PTR     next_trigger;
   TRIGGER_PTR     prev_trigger;
   char            name[32];
   int             aflag;
   int             hidden;
   unsigned long   experiment_mask;
} TRIGGER;

typedef struct hidden_list
{
   unsigned int    time;
   char            cmd[256];
   char            name[32];
   char            gui[32];
   int             aflag;
   int             hidden;
   unsigned long   experiment_mask;
   struct hidden_list *next;
   struct hidden_list *prev;
} HIDDEN_LIST;

#define MAX_INFORM 256
typedef struct inform_queque *INFORM_PTR;
typedef struct inform_queque
{
   int                 socket;
   char                host[128];
   struct sockaddr_in  send_addr;
   int                 port;
   INFORM_PTR          next_inform;
   INFORM_PTR          prev_inform;
} INFORM;


extern int read_tab(char *line, struct gui_tab *guitab);
extern int tpopen (int port, u_long ip);
extern int create_recv_socket(int port, struct sockaddr_in *myaddr_in);
extern int sgetline (char *fp, char *buf);

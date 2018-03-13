/******************************************************************************/
/*                                                                            */
/* tpopen - Oeffnen eines Sockets                                  */
/*                                                                            */
/* Aufruf : tpopen_serv (port, ip)                                            */
/* Return Value : ein Descriptor auf den offenen Socket, der horchen kann     */
/*                -1 im Fehlerfall                                            */
/*                                                                            */
/* (Ursula Hilgers)                                                           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* $Header: tpopen_serv.c,v 1.2 93/02/03 15:40:31 tine Exp $		      */
/*                                                                            */
/******************************************************************************/

# include <stdio.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#include <errno.h>

struct linger linger  = {1,1};

int tpopen (int port, u_long ip)
{
        struct sockaddr_in 	serv_addr;
	int			ls;  /* Filedescriptor auf geoeffneten Socket */
        int                     val = 1;

        void			*memset ();

/* Initialize Struktur */
        memset ((void*)((char *) &serv_addr), 0,
                (size_t) sizeof (struct sockaddr_in));

        serv_addr.sin_family = AF_INET;       /* address family - IPaddresses */
/*********
        serv_addr.sin_addr.s_addr = ip;
**********/
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = port;

/* create socket */
        if ((ls = socket (AF_INET, SOCK_STREAM, 0)) == -1)
        {
           fprintf (stderr, "cannot create socket\n");
           return (-1);
        }
/**********
        setsockopt (ls, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
*********/
        setsockopt (ls, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (bind(ls, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) == -1)
        {
           fprintf (stderr, "unable to bind address: %d \n", port);
           return (-1);
	}
        if (listen (ls, SOMAXCONN) == -1)
        {
           fprintf (stderr, "server socket unable to listen\n");
           return (-1);
        }
	return (ls);
}

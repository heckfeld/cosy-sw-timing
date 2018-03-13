 /******************************************************************************/
/*                                                                            */
/* $Header: sw_timing.c,v 1.23 93/03/30 16:19:12 tine Exp $                      */
/*                                                                            */
/******************************************************************************/
#define EXTERN extern

# include <stdio.h>
#include <unistd.h>
# include <string.h>             /* fuer 'strlen()', 'memset()' */

# include <fcntl.h>
# include <sw_timing.h>

int
read_tab(char *line, struct gui_tab *guitab)
{
  char *s = line;
  char *u;
  char tmp[256];
  char *t = tmp;
  int count = 0;
  int i, j, k, ix;
  
  while ((*s != '#') && (*s != '\n'))
     *t++ = *s++;
  *t = '\0';
  s = tmp;

  for (j = 1; j <= 7; j++)
  {
     if (*s == '\0')
        return (count);

     while ( (*s == ' ') || (*s =='\t') )
     {
        s++;
     }
     if (*s == '\0') 
        return (count);

     switch (count)
     {
         case 0:
            k = sscanf(s, "%s", &guitab->name);
            break;
         case 1:
            k = sscanf(s, "%d", &guitab->addr_port);
            break;
         case 2:
            k = sscanf(s, "%d", &guitab->sw_timing_port);
            break;
         case 3:
            k = sscanf(s, "%d", &guitab->ust_port);
            break;
         case 4:
            k = sscanf(s, "%s", &guitab->host);
            break;
         case 5:
            k = sscanf(s, "%s", &guitab->display);
            break;
         case 6:
            if (*s != '"') 
            {
               k = 0;
               break;
            }
            s++;
            u = guitab->title;
            for (ix = 0; ix < 63; ix++)
            {
               if (*s == '"')
                  break;
               *u++ = *s++;
            }
            *u = '\0';
            k = strlen(guitab->title);
            break;
     }
     if (k)
        count++;
     if (count < 7)
        while ( (*s != ' ') && (*s != '\t') )
           s++;
  }

  while ( (*s == ' ') && (*s =='\t') )
  {
     if (*s == '\0') 
        return (count);
     s++;
  }

  return (count);
}

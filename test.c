
#include <stdio.h>



static FILE *infp;

static void __attribute__((constructor)) init_infp(void)
{
 infp = stdin; 
} 

main()
{
   exit (1);
}

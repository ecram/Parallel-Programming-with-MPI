/* ---------------------------------------------------------------
 * MPI pi Calculation Example - C Version 
 * Collective Communication example
 * FILE: pi.c
 * OTHER FILES: dboard.c
 *
 *   This version uses one MPI routine to collect results
 *
 * AUTHOR: Roslyn Leibensperger. Converted to MPI: George L. Gusciora 
 *   (1/25/95)
 * LAST REVISED: 06/07/96 Blaise Barney
 * --------------------------------------------------------------- */


#include <stdlib.h>
#include <stdio.h>
void srandom (unsigned seed);
double dboard (int darts);
#define DARTS 5000      /* number of throws at dartboard */
#define ROUNDS 10       /* number of times "darts" is iterated */
#define MASTER 0        /* task ID of master task */

int main(argc,argv)
int argc;
char *argv[];
{
double	homepi,         /* value of pi calculated by current task */
	pisum,	        /* sum of tasks' pi values */
	pi,	        /* average of pi after "darts" is thrown */
	avepi;	        /* average pi value for all iterations */
int	taskid,	        /* task ID - also used as seed number */
	numtasks,       /* number of tasks */
	rc,             /* return code */
	i;
MPI_Status status;

   /* Obtain number of tasks and task ID */




   if (rc != 0)
      printf ("error initializing MPI and obtaining task ID information\n");
   else
      printf ("task ID = %d\n", taskid);

   /* Set seed for random number generator equal to task ID */
   srandom (taskid);

   avepi = 0;
   for (i = 0; i < ROUNDS; i++)
   {
      /* All tasks calculate pi using dartboard algorithm */
      homepi = dboard(DARTS);
      /* Use MPI Routine across all tasks 
       */




      if (rc != 0)
         printf("%d: failure on mpc_reduce\n", taskid);

      /* Master computes average for this iteration and all iterations */
      if (taskid == MASTER)
      {
         pi = pisum/numtasks;
         avepi = ((avepi * i) + pi)/(i + 1); 
         printf("   After %3d throws, average value of pi = %10.8f\n",
                   (DARTS * (i + 1)),avepi);
      }    
   } 



   return 0;
}

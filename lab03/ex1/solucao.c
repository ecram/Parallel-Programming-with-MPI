/* ----------------------------------------------------------------------
 
    This program tests the use of MPI Collective Communications
    subroutines.  The structure of the program is:
 
       --  Master node queries for random number seed
       --  The seed is sent to all nodes (Lab project)
       --  Each node calculates one random number based on the seed
           and the rank 
       --  The node with highest rank calculates the mean value
           of the random numbers (Lab project)
       --  4 more random numbers are generated by each node
       --  The maximum value and the standard deviation of all
           generated random numbers are calculated, and the
           results are made available to all nodes (Lab project)
 
    Also provided is a service routine GetStats(rnum,N,data), where
 
          rnum:  array of random numbers (INPUT)
          N:     number of elements in rnum (INPUT)
          outd:  array of size 2 containing the maximum value and
                 standard deviation (OUTPUT)
 
  ---------------------------------------------------------------------- */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

int GetStats(float* rnum, int n, float* rval);

int main(int argc, char** argv)
{
  int 	        numtasks,taskid, ii;
  unsigned int  seed;
  float	        randnum[5], sum, meanv, rnum[100], rval[2];
  MPI_Status    status;
  FILE* fp;

  MPI_Init( &argc, &argv );
  MPI_Comm_rank( MPI_COMM_WORLD, &taskid );
  MPI_Comm_size( MPI_COMM_WORLD, &numtasks );
  fp = fopen( "c.data", "w" );

  if( taskid == 0 )    /*  Get random number seed */
  {
    printf( " Enter random number as positive integer\n" );
    scanf( "%u", &seed);
  }
 
/* 
   ================================================
   Project:  Send seed from task 0 to all nodes.   
   ================================================ 
*/
  MPI_Bcast( &seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD );

  printf( "\n Task %d after broadcast; seed = %u", taskid, seed );
  srand( seed + taskid );

  randnum[0] = 100.*(float)rand()/(float)RAND_MAX; /* Get a random number */

/* 
   ============================================================== 
   Project:  Have the node with highest rank calculate the      
             mean value of the random numbers and store result  
             in the variable "meanv".                           
   ============================================================== 
*/
  MPI_Reduce( randnum, &sum, 1, MPI_FLOAT, MPI_SUM, numtasks-1, 
              MPI_COMM_WORLD);
  meanv = sum/(float)numtasks;

  printf( "\n Task %d after mean value; random[1] =", taskid );
  printf( "%8.3f sum = %8.3f mean = %8.3f", randnum[1], sum, meanv );

                               /* Highest task writes out mean value */
  if( taskid == (numtasks-1) )
    fprintf( fp, " For seed = %d    mean value = %10.3f\n", seed, meanv );

                                /*  Generate 4 more random numbers */
  for( ii=1; ii < 5; ii++ )
    randnum[ii] = 100.*(float)rand()/(float)RAND_MAX; 

/* 
   ==================================================================
   Project:  Calculate the maximum value and standard deviation of 
             all random numbers generated, and make results known
             to all nodes.
   Method 1:  Use GATHER followed by BCAST
   Method 2:  Use ALLGATHER
   ================================================================== 
*/
  /*   ------  Method 1   ----------- */

  MPI_Gather( randnum, 5, MPI_FLOAT, rnum, 5, MPI_FLOAT, 0, MPI_COMM_WORLD );
  if( taskid == 0 ) 
    GetStats( rnum, 5*numtasks, rval);
  MPI_Bcast( rval, 2, MPI_FLOAT, 0, MPI_COMM_WORLD );

  printf( "\n Task %d after Method 1, rnum(1:5) =", taskid );
  for( ii=1; ii < 5; ii++ )
    printf( " %8.3f", rnum[ii] ); 
  if( taskid == (numtasks-1)) 
    fprintf( fp, " (Max, S.D.) = %10.3f%10.3f\n", rval[0], rval[1]);

  /*   ------  Method 2   -----------  */

  MPI_Allgather( randnum, 5, MPI_FLOAT, rnum, 5, MPI_FLOAT, MPI_COMM_WORLD );
  GetStats( rnum, 5*numtasks, rval);

  printf( "\n Task %d after Method 2, rnum(1:5) =", taskid );
  for( ii=1; ii < 5; ii++ )
    printf( " %8.3f", rnum[ii] );
  printf( "\n" );
  if( taskid == (numtasks-1)) 
    fprintf( fp," (Max, S.D.) = %10.3f%10.3f\n", rval[0], rval[1]);

  fclose( fp );
  MPI_Finalize();
}

 
/* ----------------------------------------------------------------------
   Service routine GetStats( rnum, N, data), where
 
       rnum:  array of random numbers (INPUT)
       N:     number of elements in rnum (INPUT)
       outd:  array of size 2 containing the maximum value and
              standard deviation (OUTPUT)
 
  ---------------------------------------------------------------------- */

int GetStats(float* rnum, int N, float* outd)
{
  float  sum, meanv, sdev;
  int    ii;

  sum = 0.;
  *outd = 0.;

  for( ii = 0 ; ii < N ; ii++ )
  {
    sum += *(rnum + ii);
    if( *(rnum + ii) > *outd ) 
      *outd = *(rnum + ii);
  }

  meanv = sum/(float)N;
  sdev = 0.;
  for( ii = 0; ii < N; ii++ )
    sdev += (*(rnum + ii) - meanv) * (*(rnum + ii) - meanv);

  *(outd + 1) = sqrt( sdev/(float)N );
}

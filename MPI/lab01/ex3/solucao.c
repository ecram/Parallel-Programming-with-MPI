/* 
 * hello.ex2.c 
 * Parallel version using MPI calls
 * Modified from basic version so that workers send back 
 * a message to the master, who prints out a message 
 * for each worker 
 * In addition, the master now sends out two messages to each worker, with 
 * two different tags, and each worker receives the messages 
 * in reverse order.  Each worker returns a message to the master 
 * with a tag equal to the worker's rank, and the master receives 
 * that message using the tag. 
 * RLF 10/23/95
*/
#include <stdio.h>  
#include <string.h>  
#include <stdlib.h>
#include "mpi.h"
main(int argc, char **argv )
{
	char message1[7], message2[7];
	int i,rank, size, type=99;
        int type2=100;
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == 0) {
	   strcpy(message1, "Hello,");
           strcpy(message2, "world");
	   for (i=1; i<size; i++) {
	   MPI_Send(message1, 7, MPI_CHAR, i, 
               type, MPI_COMM_WORLD);
  	   MPI_Send(message2, 7, MPI_CHAR, i, 
               type2, MPI_COMM_WORLD); 
           }
           printf("node %d : %.7s %.7s\n", rank, message1, message2);
	} 
  	else {
  	  MPI_Recv(message1, 7, MPI_CHAR, 0,
  	       type2, MPI_COMM_WORLD, &status); 
	  MPI_Recv(message2, 7, MPI_CHAR, 0,
	       type, MPI_COMM_WORLD, &status);
          printf("node %d : %.7s , %.7s\n", rank, message1, message2);

          
          }
      	MPI_Finalize();
}

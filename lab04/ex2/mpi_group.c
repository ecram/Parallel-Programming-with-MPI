/******************************************************************************
* MPI tutorial example code: Groups/Communicators
* FILE: mpi_group.c
* AUTHOR: Blaise Barney
* LAST REVISED:
******************************************************************************/
#include "mpi.h"
#include <stdio.h>
#define NPROCS 8

int main(argc,argv)
int argc;
char *argv[];  {
int        rank, new_rank, sendbuf, recvbuf, numtasks,
           ranks1[4]={0,1,2,3}, ranks2[4]={4,5,6,7};
MPI_Group  orig_group, new_group;
MPI_Comm   new_comm;

MPI_Init(&argc,&argv);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

if (numtasks != NPROCS) {
  printf("Must specify %d. tasks. Terminating.\n",NPROCS);
  MPI_Finalize();
  exit(0);
  }

sendbuf = rank;

/* Extract the original group handle */

=====> Rotina MPI - Identificar o grupo

/* Divide tasks into two distinct groups based upon rank */
if (rank < NPROCS/2) {

=====> Rotina MPI - Define novo grupo com ranks1

  }
else {

=====> Rotina MPI - Define novo grupo com ranks2

  }

/* Create new new communicator and then perform collective communications */

=====> Rotina MPI - Define novo communicator

MPI_Allreduce(&sendbuf, &recvbuf, 1, MPI_INT, MPI_SUM, new_comm);


=====> Rotina MPI - Identifica os processos para os novos grupos

printf("rank= %d newrank= %d recvbuf= %d\n",rank,new_rank,recvbuf);

MPI_Finalize();
}

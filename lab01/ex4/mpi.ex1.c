/******************************************************************************
* FILE: mpl.ex1.c
* DESCRIPTION: 
*   In this simple example, the master task initiates numtasks-1 number of
*   worker tasks.  It then distributes an equal portion of an array to each 
*   worker task.  Each worker task receives its portion of the array, and 
*   performs a simple value assignment to each of its elements. The value 
*   assigned to each element is simply that element's index in the array+1.  
*   Each worker task then sends its portion of the array back to the master 
*   task.  As the master receives back each portion of the array, selected 
*   elements are displayed. 
* AUTHOR: Blaise Barney
* LAST REVISED:  09/14/93 for latest API changes  Blaise Barney
* LAST REVISED:  01/10/94 changed API to MPL      Stacy Pendell
* CONVERTED TO MPI: 11/12/94 by                   Xianneng Shen
****************************************************************************/

#include <stdio.h>

====>    INCLUDE MPI

#define	ARRAYSIZE	60000	
#define MASTER		0	/* taskid of first process */

MPI_Status status;
main(int argc, char **argv) 
{
int	numtasks, 		/* total number of MPI process in partitiion */
	numworkers,		/* number of worker tasks */
	taskid,			/* task identifier */
	dest,			/* destination task id to send message */
	index, 			/* index into the array */
	i, 			/* loop variable */
       	arraymsg = 1, 		/* setting a message type */
   	indexmsg = 2,		/* setting a message type */
	source,			/* origin task id of message */
	chunksize; 		/* for partitioning the array */
float	data[ARRAYSIZE], 	/* the intial array */
	result[ARRAYSIZE];	/* for holding results of array operations */

/************************* initializations ***********************************
* Find out how many tasks are in this partition and what my task id is.  Then
* define the number of worker tasks and the array partition size as chunksize. 
* Note:  For this example, the MP_PROCS environment variable should be set
* to an odd number...to insure even distribution of the array to numtasks-1
* worker tasks.
******************************************************************************/

====>    ROTINA MPI DE INICIALIZACAO (&argc, &argv)
====>    ROTINA MPI DE IDENTIFICACAO (&taskid)
====>    ROTINA MPI PARA DETERMINAR O NUMERO DE PROCESSOS (&numtasks)

numworkers = numtasks-1;
chunksize = (ARRAYSIZE / numworkers);

/**************************** master task ************************************/
if (taskid == MASTER) {
  printf("\n*********** Starting MPI Example 1 ************\n");
  printf("MASTER: number of worker tasks will be= %d\n",numworkers);
  fflush(stdout);

  /* Initialize the array */
  for(i=0; i<ARRAYSIZE; i++) 
    data[i] =  0.0;
  index = 0;

  /* Send each worker task its portion of the array */
  for (dest=1; dest<= numworkers; dest++) {
    printf("Sending to worker task= %d\n",dest);
    fflush(stdout);

====>    ROTINA MPI DE ENVIO DE DADOS (&index,dest)
====>    ROTINA MPI DE ENVIO DE DADOS (&data[index],chunksize,dest)

    index = index + chunksize;
    }

  /* Now wait to receive back the results from each worker task and print */
  /* a few sample values */
  for (i=1; i<= numworkers; i++) {
    source = i;

====>    ROTINA MPI PARA RECEBER DADOS (&index,source,&status)
====>    ROTINA MPI PARA RECEBER DADOS (&result[index],chunksize,source,&status)

    printf("---------------------------------------------------\n");
    printf("MASTER: Sample results from worker task = %d\n",source);
    printf("   result[%d]=%f\n", index, result[index]);
    printf("   result[%d]=%f\n", index+100, result[index+100]);
    printf("   result[%d]=%f\n\n", index+1000, result[index+1000]);
    fflush(stdout);
    }

  printf("MASTER: All Done! \n");
  }


/**************************** worker task ************************************/
if (taskid > MASTER) {
  /* Receive my portion of array from the master task */
  source = MASTER;

====>    ROTINA MPI PARA RECEBER DADOS (&index,source,&status)
====>    ROTINA MPI PARA RECEBER DADOS (&result[index],chunksize,source,&status)

  /* Do a simple value assignment to each of my array elements */
  for(i=index; i < index + chunksize; i++)
    result[i] = i + 1;

  /* Send my results back to the master task */

====>    ROTINA MPI DE ENVIO DE DADOS (&index,MASTER)
====>    ROTINA MPI DE ENVIO DE DADOS (&result[index],chunksize,MASTER)

  }

====>    ROTINA MPI DE FINALIZACAO

}

/* -----------------------------------------------------------------------
 
    create.soln.c
    Lab solution for MPI Groups and Communicator Management
 
    There are 2 parts to the project:
         1.  Build 2 groups, one for even-numbered tasks (starting
             with task 0), one for odd-numbered tasks.
         2.  Do a sum reduction for each group.
 
    AUTHOR: S. Hotovy 12/95
    REVISED: R. Leibensperger 4/96

  -------------------------------------------------------------------- */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

void set_group(MPI_Comm[], MPI_Group[], int);

      main(int argc, char **argv)
{
      int ntask, rank_in_world, rank_in_newgrp;
      MPI_Comm newcomm[2];
      MPI_Group newgrp[2];
      float xsin, xsum;
      FILE *fpeven, *fpodd;

      MPI_Init(&argc, &argv);
/*
   ==================================================================
    Establish number of tasks and rank of this task in MPI_COMM_WORLD
   ==================================================================
*/
      MPI_Comm_size(MPI_COMM_WORLD,&ntask);
      MPI_Comm_rank(MPI_COMM_WORLD,&rank_in_world);
/*
   ==============================
    Call routine to set up groups
   ==============================
*/
      set_group(newcomm, newgrp, ntask);
      xsin = fabs(sin(151.*(float)(rank_in_world+10)));
/*
  ==================================================================
    Project:  Sum all values of xsin on even-numbered tasks

    First:    Learn your rank in this new group (MPI_UNDEFINED if 
              you are not a member), assign to the variable 
	      rank_in_newgrp
    Second:   Members accumulate the sum across the group, and store
              it as xsum on the task with rank 0  
  ==================================================================
*/
      MPI_Group_rank (newgrp[0], &rank_in_newgrp);

      if (rank_in_newgrp != MPI_UNDEFINED)  {

        MPI_Reduce(&xsin,&xsum,1,MPI_FLOAT,MPI_SUM,0,newcomm[0]);

	if (rank_in_newgrp == 0)  {
	  fpeven = fopen("even.rep","w");
	  fprintf(fpeven,"Sum of even tasks: %12.5f\n",xsum);
	  fclose (fpeven);  }
      }
/*
  ==================================================================
    Project:  Sum all values of xsin on odd-numbered tasks

    First:    Learn your rank in this new group (MPI_UNDEFINED if 
              you are not a member), assign to the variable 
	      rank_in_newgrp
    Second:   Members accumulate the sum across the group, and store
              it as xsum on the task with rank 0
  ==================================================================
*/
      MPI_Group_rank (newgrp[1], &rank_in_newgrp);

      if (rank_in_newgrp != MPI_UNDEFINED)  {

        MPI_Reduce(&xsin,&xsum,1,MPI_FLOAT,MPI_SUM,0,newcomm[1]);

	if (rank_in_newgrp == 0)  {
	  fpodd = fopen("odd.rep","w");	
	  fprintf(fpodd,"Sum of odd tasks: %12.5f\n",xsum);
	  fclose (fpodd);  }
      }
      MPI_Finalize();
}

 
      void set_group(MPI_Comm newcomm[], MPI_Group newgrp[], int ntask)
      {
      int list[100], ktr, i;
      MPI_Group base_grp;
/*
   ==============================================================
    Project:  Get base group from MPI_COMM_WORLD communicator
              using the variable base_grp to hold the information
   ==============================================================
*/


==========> ROTINA MPI - Identificação do grupo principal. Armazenar em "&base_grp" 


/*
   ================================================================
    Project:  Build a group containing even-numbered tasks
              and create the corresponding communicator.
              Put the new group information in the variable 
              newgrp[0] and the communicator in variable newcomm[0]
   ================================================================
*/
      ktr = 0;
      for(i=0 ; i < ntask ; i=i+2) {
	 list[ktr] = i;
	 ktr ++;
	 }
 
===========>  ROTINA MPI: Incluir processos "list(ktr)" no grupo0 "&newgrp[0])" 


===========>  ROTINA MPI: Criar communicator "&newcomm[0]" para o grupo0
 
/*
   ================================================================
    Project:  Build a group containing odd-numbered tasks
              and create the corresponding communicator.
              Put the new group information in the variable
              newgrp[1] and the communicator in variable newcomm[1]
   ================================================================
*/
      /*  =====>  Put code here   */
 
      ktr = 0;
      for(i=1 ; i < ntask ; i=i+2) {
	 list[ktr] = i;
	 ktr ++;
	 }

===========>  ROTINA MPI: Incluir processos "list(ktr)" no grupo1 "&newgrp[1]" 


===========>  ROTINA MPI: Criar communicator "&newcomm[1]" para o grupo1

   }

! -----------------------------------------------------------------------
!
!   create.soln.f
!   Lab solution for MPI Groups and Communicator Management
!
!   There are 2 parts to the project:
!        1.  Build 2 groups, one for even-numbered tasks (starting
!            with task 0), one for odd-numbered tasks.
!        2.  Do a sum reduction for each group.
!
!   AUTHOR: S. Hotovy 12/95
!   REVISED: R. Leibensperger 4/96
!
! -----------------------------------------------------------------------
!
      Program Test
      include 'mpif.h'
      integer newgrp(2), newcomm(2), 
     1        ntask, rank_in_world, rank_in_newgrp, ierr
      call MPI_INIT(ierr)
!
!  ==================================================================
!   Establish number of tasks and rank of this task in MPI_COMM_WORLD
!  ==================================================================
!
      call MPI_COMM_SIZE(MPI_COMM_WORLD, ntask, ierr)
      call MPI_COMM_RANK(MPI_COMM_WORLD, rank_in_world, ierr)
!
!  ==============================
!   Call routine to set up groups
!  ==============================
!
      call set_group(newcomm, newgrp, ntask)
      xsin = ABS(SIN(151.*float(rank_in_world+10)))
!
! ==================================================================
!   Project:  Sum all values of xsin on even-numbered tasks
!
!   First:    Learn your rank in this new group (MPI_UNDEFINED if 
!             you are not a member), assign to the variable 
!	      rank_in_newgrp
!   Second:   Members accumulate the sum across the group, and store
!             it as xsum on the task with rank 0  
! ==================================================================
!
      call MPI_GROUP_RANK(newgrp(1), rank_in_newgrp, ierr)
!      
      if (rank_in_newgrp .NE. MPI_UNDEFINED) then
         call MPI_REDUCE(xsin,xsum,1,MPI_REAL,MPI_SUM,0,newcomm(1),ierr)
         if (rank_in_newgrp .EQ. 0) then
            open (unit=10, file='even.rep', status='unknown')
            write(10,'(a,f12.5)') 'Sum of even tasks =',xsum
            close (10)
         end if
      end if
!
! ==================================================================
!   Project:  Sum all values of xsin on odd-numbered tasks
!
!   First:    Learn your rank in this new group (MPI_UNDEFINED if 
!             you are not a member), assign to the variable 
!	      rank_in_newgrp
!   Second:   Members accumulate the sum across the group, and store
!             it as xsum on the task with rank 0  
! ==================================================================
!
      call MPI_GROUP_RANK(newgrp(2), rank_in_newgrp, ierr)
!      
      if (rank_in_newgrp .NE. MPI_UNDEFINED) then
         call MPI_REDUCE(xsin,xsum,1,MPI_REAL,MPI_SUM,0,newcomm(2),ierr)
         if (rank_in_newgrp .EQ. 0) then
            open (unit=11, file='odd.rep', status='unknown')
            write(11,'(a,f12.5)') 'Sum of odd tasks =',xsum
            close (11)
            end if
         end if
!
      call MPI_FINALIZE(ierr)
      stop
      end
!
!
      subroutine set_group(newcomm, newgrp, ntask)
      include 'mpif.h'
      integer newcomm(2), newgrp(2), ntask,
     1        list(ntask), base_grp, ktr, i
!
!  ==============================================================
!   Project:  Get base group from MPI_COMM_WORLD communicator
!             using the variable base_grp to hold the information
!  ==============================================================
!


==========> ROTINA MPI - Identificação do grupo principal. Armazenar em "base_grp"


!
!  ================================================================
!   Project:  Build a group containing even-numbered tasks
!             and create the corresponding communicator.
!             Put the new group information in the variable
!             newgrp(1) and the communicator in variable newcomm(1)
!  ================================================================
!
      ktr = 0
      do i=0,ntask-1,2
	 ktr = ktr + 1
	 list(ktr) = i
      end do

===========>  ROTINA MPI: Incluir processos "list(ktr)" no grupo1 "newgrp(1)" 


===========>  ROTINA MPI: Criar communicator "newcomm(1)" para o grupo1


!
!
!  ================================================================
!   Project:  Build a group containing odd-numbered tasks
!             and create the corresponding communicator.
!             Put the new group information in the variable
!             newgrp(2) and the communicator in variable newcomm(2)
!  ================================================================
!
      ktr = 0
      do i=1,ntask-1,2
	 ktr = ktr + 1
	 list(ktr) = i
      end do

===========>  ROTINA MPI: Incluir processos "list(ktr)" no grupo2 "newgrp(2)" 


===========>  ROTINA MPI: Criar communicator "newcomm(2)" para o grupo2

!
      return
      end

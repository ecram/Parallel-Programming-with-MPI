C ****************************************************************************
C  FILE: mpi_pi_reduce.f 
C  OTHER FILES: dboard.f
C  DESCRIPTION:  
C    MPI pi Calculation Example - Fortran Version
C    Collective Communications examples
C    This program calculates pi using a "dartboard" algorithm.  See
C    Fox et al.(1988) Solving Problems on Concurrent Processors, vol.1
C    page 207.  All processes contribute to the calculation, with the
C    master averaging the values for pi. SPMD Version:  Conditional 
C    statements check if the process is the master or a worker.
C    This version uses mp_reduce to collect results
C    Note: Requires Fortran90 compiler due to random_number() function
C  AUTHOR: Blaise Barney. Adapted from Ros Leibensperger, Cornell Theory
C    Center. Converted to MPI: George L. Gusciora, MHPCC (1/95)  
C  LAST REVISED: 01/23/09 Blaise Barney
C ****************************************************************************
C Explanation of constants and variables used in this program:
C   DARTS          = number of throws at dartboard 
C   ROUNDS         = number of times "DARTS" is iterated 
C   MASTER         = task ID of master task
C   taskid         = task ID of current task 
C   numtasks       = number of tasks
C   homepi         = value of pi calculated by current task
C   pisum          = sum of tasks' pi values 
C   pi 	           = average of pi for this iteration
C   avepi          = average pi value for all iterations 
C   seednum        = seed number - based on taskid

      program pi_reduce
      include 'mpif.h'

      integer DARTS, ROUNDS, MASTER
      parameter(DARTS = 5000) 
      parameter(ROUNDS = 10)
      parameter(MASTER = 0)

      integer 	taskid, numtasks, i, status(MPI_STATUS_SIZE)
      real*4	seednum
      real*8 	homepi, pi, avepi, pisum, dboard
      external  d_vadd

C     Obtain number of tasks and task ID
      call MPI_INIT( ierr )
      call MPI_COMM_RANK( MPI_COMM_WORLD, taskid, ierr )
      call MPI_COMM_SIZE( MPI_COMM_WORLD, numtasks, ierr )
      write(*,*)'task ID = ', taskid

      avepi = 0

      do 40 i = 1, ROUNDS
C     Calculate pi using dartboard algorithm 
        homepi = dboard(DARTS)

C     Use mp_reduce to sum values of homepi across all tasks
C     Master will store the accumulated value in pisum
C     - homepi is the send buffer
C     - pisum is the receive buffer (used by the receiving task only)
C     - MASTER is the task that will receive the result of the reduction
C       operation
C     - d_vadd is a pre-defined reduction function (double-precision
C       floating-point vector addition)
C     - allgrp is the group of tasks that will participate
        call MPI_REDUCE( homepi, pisum, 1, MPI_DOUBLE_PRECISION, 
     &                   MPI_SUM, MASTER, MPI_COMM_WORLD, ierr )
C     Master computes average for this iteration and all iterations 
        if (taskid .eq. MASTER) then
          pi = pisum/numtasks
          avepi = ((avepi*(i-1)) + pi) / i
          write(*,32) DARTS*i, avepi
 32       format('   After',i6,' throws, average value of pi = ',f10.8) 
        endif
 40   continue
      call MPI_FINALIZE(ierr)
      end


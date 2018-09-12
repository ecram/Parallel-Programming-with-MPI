C  ------------------------------------------------------------------------
C  MPI pi Calculation Example - Fortran Version
C  Collective Communications examples
C  FILE: pi.f 
C  OTHER FILES: dboard.f
C
C  This version uses one MPI routine to collect results
C 
C  AUTHOR: Roslyn Leibensperger. Converted to MPI: George L. Gusciora (1/23/95)
C  LAST REVISED: 12/14/95 Blaise Barney
C  ------------------------------------------------------------------------
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
C  ------------------------------------------------------------------------

      program pi_reduce


      integer DARTS, ROUNDS, MASTER
      parameter(DARTS = 5000) 
      parameter(ROUNDS = 10)
      parameter(MASTER = 0)

      integer 	taskid, numtasks, i, status(MPI_STATUS_SIZE)
      real*4	seednum
      real*8 	homepi, pi, avepi, pisum, dboard
      external  d_vadd

C     Obtain number of tasks and task ID



      write(*,*)'task ID = ', taskid

C     Use the task id to set the seed number for the random number generator.
      seednum = real(taskid)
      call srand(seednum)
      avepi = 0

      do 40 i = 1, ROUNDS
C     Calculate pi using dartboard algorithm 
        homepi = dboard(DARTS)

C     Use mpi routine  across all tasks




C     Master computes average for this iteration and all iterations 
        if (taskid .eq. MASTER) then
          pi = pisum/numtasks
          avepi = ((avepi*(i-1)) + pi) / i
          write(*,32) DARTS*i, avepi
 32       format('   After',i6,' throws, average value of pi = ',f10.8) 
        endif
 40   continue




      end

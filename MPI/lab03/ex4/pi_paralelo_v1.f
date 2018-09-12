C ****************************************************************************
C  FILE: mpi_pi_send.f
C  OTHER FILES: dboard.f
C  DESCRIPTION:  
C    MPI pi Calculation Example - Fortran Version 
C    Point-to-Point Communication example
C    This program calculates pi using a "dartboard" algorithm.  See
C    Fox et al.(1988) Solving Problems on Concurrent Processors, vol.1
C    page 207.  All processes contribute to the calculation, with the
C    master averaging the values for pi. SPMD Version:  Conditional 
C    statements check if the process is the master or a worker.
C    This version uses low level sends and receives to collect results 
C    Note: Requires Fortran90 compiler due to random_number() function
C AUTHOR: Blaise Barney. Adapted from Ros Leibensperger, Cornell Theory
C   Center. Converted to MPI: George L. Gusciora, MHPCC (1/95)  
C  LAST REVISED: 01/23/09 Blaise Barney
C ****************************************************************************
C Explanation of constants and variables used in this program:
C   DARTS          = number of throws at dartboard 
C   ROUNDS         = number of times "DARTS" is iterated 
C   MASTER         = task ID of master task
C   taskid         = task ID of current task 
C   numtasks       = number of tasks
C   homepi         = value of pi calculated by current task
C   pi             = average of pi for this iteration
C   avepi          = average pi value for all iterations 
C   pirecv         = pi received from worker 
C   pisum          = sum of workers' pi values 
C   seednum        = seed number - based on taskid
C   source         = source of incoming message
C   mtype          = message type 
C   i, n           = misc

      program pi_send 
      include 'mpif.h'

      integer DARTS, ROUNDS, MASTER
      parameter(DARTS = 5000) 
      parameter(ROUNDS = 10)
      parameter(MASTER = 0)

      integer 	taskid, numtasks, source, mtype, i, n,
     &          status(MPI_STATUS_SIZE)
      real*4	seednum
      real*8 	homepi, pi, avepi, pirecv, pisum, dboard

C     Obtain number of tasks and task ID
      call MPI_INIT( ierr )
      call MPI_COMM_RANK( MPI_COMM_WORLD, taskid, ierr )
      call MPI_COMM_SIZE( MPI_COMM_WORLD, numtasks, ierr )
      write(*,*)'task ID = ', taskid

      avepi = 0

      do 40 i = 1, ROUNDS
C     Calculate pi using dartboard algorithm 
      homepi = dboard(DARTS)

C     ******************** start of worker section ***************************
C     All workers send result to master.  Steps include:  
C     -set message type equal to this round number
C     -set message size to 8 bytes (size of real8)
C     -send local value of pi (homepi) to master task
      if (taskid .ne. MASTER) then
        mtype = i 
        sbytes = 8	
        call MPI_SEND( homepi, 1, MPI_DOUBLE_PRECISION, MASTER, i, 
     &                 MPI_COMM_WORLD, ierr )

C     ******************** end of worker section *****************************
      else
C     ******************** start of master section **************************
C     Master receives messages from all workers.  Steps include:
C     -set message type equal to this round 
C     -set message size to 8 bytes (size of real8)
C     -receive any message of type mytpe
C     -keep running total of pi in pisum
C     Master then calculates the average value of pi for this iteration 
C     Master also calculates and prints the average value of pi over all 
C     iterations 
        mtype = i	
        sbytes = 8
        pisum = 0
        do 30 n = 1, numtasks-1
          call MPI_RECV( pirecv, 1, MPI_DOUBLE_PRECISION,
     &       MPI_ANY_SOURCE, mtype, MPI_COMM_WORLD, status, ierr )
          pisum = pisum + pirecv
 30     continue
        pi = (pisum + homepi)/numtasks
        avepi = ((avepi*(i-1)) + pi) / i
        write(*,32) DARTS*i, avepi
 32     format('   After',i6,' throws, average value of pi = ',f10.8) 
C    ********************* end of master section ****************************
      endif
 40   continue
      call MPI_FINALIZE(ierr)
      end

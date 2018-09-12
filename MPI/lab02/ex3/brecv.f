C  -----------------------------------------------------------------------
C  Code:   brecv.f
C  Lab:    MPI Point-to-Point Communication
C          This program demonstrates that replacing a blocking receive
C          with a non-blocking receive earlier in the program can decrease
C          the synchronization time on the corresponding send.
C  Usage:  brecv
C          Run on two nodes
C  Author: Roslyn Leibensperger  Last revised: 8/30/95 RYL
C ------------------------------------------------------------------------ 
      program brecv

      implicit none
      include 'mpif.h'
      
      integer MSGLEN, ITAG
      parameter ( MSGLEN = 2048, ITAG = 100 )

      real rmessage(MSGLEN)          ! message buffer
      integer irank,                 ! rank of task in communicator
     .     istatus(MPI_STATUS_SIZE), ! status of communication
     .     ierr,                     ! return status
     .     i
      real(8) itbegin, itend, dclock  ! used to measure elapsed time

      call MPI_Init ( ierr )
      call MPI_Comm_Rank ( MPI_COMM_WORLD, irank, ierr )
      print *, " Task ", irank, " initialized"

C     -----------------------------------------------------------------
C     task 0 will report the elapsed time for a blocking send
C     -----------------------------------------------------------------
      if ( irank.EQ.0 ) then
         do i = 1, MSGLEN
            rmessage(i) = 100.0
         end do
         print *, " Task ", irank, " sending message"
         itbegin = dclock ()
         call MPI_Send ( rmessage, MSGLEN, MPI_REAL, 1, ITAG, 
     .        MPI_COMM_WORLD, ierr)
         itend = dclock ()
         print *, " Elapsed time for send = ", 
     .       int((itend - itbegin)*1e6), " usec"

C     -----------------------------------------------------------------
C     task 1 sleeps for 10 seconds, and then calls a blocking receive.
C     the sleep is intended to simulate time spent in useful computation
C     -----------------------------------------------------------------
      else if ( irank.EQ.1 ) then
         do i = 1, MSGLEN
            rmessage(i) = -100.0
         end do
         call new_sleep (10)
         call MPI_Recv ( rmessage, MSGLEN, MPI_REAL, 0, ITAG,
     .        MPI_COMM_WORLD, istatus, ierr)
         print *, " Task ", irank, " received message "
      end if

      call MPI_Finalize (ierr)
      end

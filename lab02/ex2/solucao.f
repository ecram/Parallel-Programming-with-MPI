C  -----------------------------------------------------------------------
C  Code:   fixed.f
C  Lab:    MPI Point-to-Point Communication
C          Solution program showing the use of a non-blocking send
C          to eliminate deadlock.
C  Usage:  fixed
C          Run on two nodes
C  Author: Roslyn Leibensperger  Last revised: 8/30/95 RYL
C ------------------------------------------------------------------------ 
      program fixed

      implicit none
      include 'mpif.h'
      
      integer MSGLEN, ITAG_A, ITAG_B
      parameter ( MSGLEN = 204800, ITAG_A = 100, ITAG_B = 200 )

      real rmessage1(MSGLEN),        ! message buffers
     .     rmessage2(MSGLEN)
      integer irank,                 ! rank of task in communicator
     .     idest, isrc,              ! rank in communicator of destination
                                     ! and source tasks
     .     isend_tag, irecv_tag,     ! message tags
     .     irequest,                 ! handle for pending communication
     .     istatus(MPI_STATUS_SIZE), ! status of communication
     .     ierr,                     ! return status
     .     i

      call MPI_Init ( ierr )
      call MPI_Comm_Rank ( MPI_COMM_WORLD, irank, ierr )
      print *, " Task ", irank, " initialized"

C     initialize message buffers
      do i = 1, MSGLEN
         rmessage1(i) = 1000
         rmessage2(i) = -1000
      end do

C     ---------------------------------------------------------------
C     each task sets its message tags for the send and receive, plus
C     the destination for the send, and the source for the receive 
C     ----------------------------------------------------------------
      if ( irank.EQ.0 ) then
         idest = 1
         isrc = 1
         isend_tag = ITAG_A
         irecv_tag = ITAG_B
      else if ( irank.EQ.1 ) then
         idest = 0
         isrc = 0
         isend_tag = ITAG_B
         irecv_tag = ITAG_A  
      end if

C     ----------------------------------------------------------------
C     send and receive messages 
C     ----------------------------------------------------------------
      print *, " Task ", irank, " has started the send"
      call MPI_Isend ( rmessage1, MSGLEN, MPI_REAL, idest, isend_tag,
     .     MPI_COMM_WORLD, irequest, ierr )
      call MPI_Recv ( rmessage2, MSGLEN, MPI_REAL, isrc, irecv_tag, 
     .     MPI_COMM_WORLD, istatus, ierr ) 
      print *, " Task ", irank, " has received the message"
      call MPI_Wait ( irequest, istatus, ierr )
      print *, " Task ", irank, " has completed the send "

      call MPI_Finalize (ierr)
      end



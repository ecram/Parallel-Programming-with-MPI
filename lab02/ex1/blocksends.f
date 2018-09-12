c  -----------------------------------------------------------------------
C  Code:   blocksends.f
C  Lab:    MPI Point-to-Point Communication
C          Time message sends that use the four blocking communication modes:
C          synchronous, ready, buffered, standard
C  Usage:  blocksends <message_length_in_number_of_reals>
C          Run on two nodes.
C  Author: Roslyn Leibensperger  Last revised: 8/29/95 RYL
C ------------------------------------------------------------------------ 
      program blocksends

      implicit none
      include 'mpif.h'

      integer NMSG,
     .     SYNC, READY, BUF, STANDARD, GO
      parameter (NMSG = 4,
     .     SYNC = 1, READY = 2, BUF = 3, STANDARD = 4, GO = 10)

      real, allocatable :: rmessage(:,:) ! message arrays
      real, allocatable :: rbuffer (:)   ! buffer required by buffered send
      integer irank,                     ! rank of task in communicator
     .     icount,                       ! number of elements in message
     .     idatatype,                    ! type of data in message
     .     idest, isource,               ! rank in communicator of destination
                                         ! and source tasks
     .     itag(NMSG), etag,             ! message tags
     .     icomm,                        ! communicator
     .     irequest(NMSG),               ! handle for pending communication
     .     istatus(MPI_STATUS_SIZE,NMSG),! status of communication
     .     imlen,                        ! message length specified by user
     .     iextent,                      ! size of datatype
     .     ibsize,                       !buffer size for buffered send, bytes
     .     iargc, ierr, i, j
      character*10 arg1
      real(8) rtc, itbegin, itend, dclock  ! used to measure elapsed time

c Valores de imlen: 1024=4K e 1025, 2048=8K e 2049, 4096=16K e 4097, 8192=32K e 8193, 16384=64K e 16385

c      imlen=1024*25000
      imlen=1025*2500

      call MPI_Init ( ierr )
      call MPI_Comm_Rank ( MPI_COMM_WORLD, irank, ierr )
      print *, " Message size = ", imlen, " reals"
      print *, " Task ", irank, " initialized"
      
      allocate ( rmessage (imlen, nmsg) )
      itag(SYNC) = SYNC
      itag(READY) = READY
      itag(BUF) = BUF
      itag(STANDARD) = STANDARD

C     -------------------------------------------------------------------
C     task 0 will receive a ready message, and then send 4 messages using
C     the different communication modes 
C     ------------------------------------------------------------------ 

      if ( irank .EQ. 0 ) then
C     initialize all message contents 
         do i = 1, nmsg
            do j = 1, imlen
               rmessage(j,i) = i + 100
            end do
         end do

C     receive empty message, indicating all receives have been posted
         icount = 0
         idatatype = MPI_INTEGER
         isource = 1
         etag = GO
         icomm = MPI_COMM_WORLD
         call MPI_Recv ( 'NULL', icount, idatatype, isource, etag, 
     .        icomm, istatus(1,1), ierr )
         print *, " Ready to send messages"

C     these message parameters apply to all succeeding sends by task 0
         icount = imlen
         idatatype = MPI_REAL
         idest = 1
         icomm = MPI_COMM_WORLD

C     time a synchronous send 
         itbegin = dclock()
         call MPI_Ssend ( rmessage(1,SYNC), icount, idatatype, idest,
     .        itag(SYNC), icomm, ierr )
         itend = dclock()
         print *, " Elapsed time for synchronous send = ", 
     .        int((itend - itbegin)*1000000), " usec"

C     time a ready send 
         itbegin = dclock()
         call MPI_Rsend ( rmessage(1,READY), icount, idatatype, idest,
     .        itag(READY), icomm, ierr )
         itend = dclock()
         print *, " Elapsed time for ready send = ",
     .    int((itend - itbegin)*1000000), " usec"

C     time overhead for buffer allocation for buffered send
         itbegin = dclock()
         call MPI_Type_extent ( MPI_REAL, iextent, ierr )
         ibsize = icount * iextent + 100
         allocate ( rbuffer (ibsize) )
         call MPI_Buffer_attach ( rbuffer, ibsize, ierr )
         itend = dclock()
         print *, " Elapsed time for buffer allocation = ",
     .        int((itend - itbegin)*1000000), " usec"  

C     time a buffered send 
         itbegin = dclock()
         call MPI_Bsend ( rmessage(1,BUF), icount, idatatype, idest,
     .        itag(BUF), icomm, ierr )
         itend = dclock()
         print *, " Elapsed time for buffered send = ",
     .        int((itend - itbegin)*1000000), " usec"
         call MPI_Buffer_detach ( rbuffer, ibsize, ierr )

C     time a standard send 
         itbegin = dclock()
         call MPI_Send ( rmessage(1,STANDARD), icount, idatatype,
     .        idest, itag(STANDARD), icomm, ierr )
         itend = dclock()
         print *, " Elapsed time for standard send = ", 
     .        int((itend - itbegin)*1000000), " usec"  
   
C     -------------------------------------------------------------------
C     task 1 will post 4 receives, and then send a "ready" message
C     -------------------------------------------------------------------

      else if ( irank .EQ. 1) then
         icount = imlen
         idatatype = MPI_REAL
         isource = 0
         icomm = MPI_COMM_WORLD

C     post non-blocking receives 
         do i = 1, NMSG
            call MPI_Irecv ( rmessage(1,i), icount, idatatype, isource, 
     .           itag(i), icomm, irequest(i), ierr )
            end do

C     send ready message indicating all receives have been posted
         icount = 0
         idatatype = MPI_INTEGER
         idest = 0
         etag = GO
         call MPI_Send ( 'NULL', icount, idatatype, idest, etag, 
     .        icomm, ierr )

C     wait for all receives to complete
         call MPI_Waitall ( NMSG, irequest, istatus, ierr )

      end if
      call MPI_Finalize(ierr)
      end

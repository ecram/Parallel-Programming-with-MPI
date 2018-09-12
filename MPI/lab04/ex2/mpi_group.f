C-----------------------------------------------------------------------------
C MPI tutorial example code: Groups/Communicators
C FILE: mpi_group.f
C AUTHOR: Blaise Barney
C LAST REVISED:
C-----------------------------------------------------------------------------
      program group
      include 'mpif.h'

      integer NPROCS
      parameter(NPROCS=8)
      integer rank, new_rank, sendbuf, recvbuf, numtasks
      integer ranks1(4), ranks2(4), ierr
      integer orig_group, new_group, new_comm
      data ranks1 /0, 1, 2, 3/, ranks2 /4, 5, 6, 7/

      call MPI_INIT(ierr)
      call MPI_COMM_RANK(MPI_COMM_WORLD, rank, ierr)
      call MPI_COMM_SIZE(MPI_COMM_WORLD, numtasks, ierr)

      if (numtasks .ne. NPROCS) then
        print *, 'Must specify ',NPROCS,' tasks. Terminating.'
        call MPI_FINALIZE(ierr)
        stop
      endif

      sendbuf = rank

C     Extract the original group handle

=====> Rotina MPI - Identificar o grupo

C     Divide tasks into two distinct groups based upon rank
      if (rank .lt. NPROCS/2) then

=====> Rotina MPI - Define novo grupo com ranks1

      else 

=====> Rotina MPI - Define novo grupo com ranks2

      endif


=====> Rotina MPI - Define novo communicator

      call MPI_ALLREDUCE(sendbuf, recvbuf, 1, MPI_INTEGER, MPI_SUM,
     &               new_comm, ierr)

=====> Rotina MPI - Identifica os processos para os novos grupos 

      print *, 'rank= ',rank,' newrank= ',new_rank,' recvbuf= ',
     &     recvbuf

      call MPI_FINALIZE(ierr)
      end

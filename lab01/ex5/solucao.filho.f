C *****************************************************************************
C FILE: mpl.ex1.f
C DESCRIPTION:
C   In this simple example, the master task initiates numtasks-1 number of
C   worker tasks.  It then distributes an equal portion of an array to each
C   worker task.  Each worker task receives its portion of the array, and
C   performs a simple value assignment to each of its elements. The value
C   assigned to each element is simply that element's index in the array+1.
C   Each worker task then sends its portion of the array back to the master
C   task.  As the master receives back each portion of the array, selected
C   elements are displayed.
C AUTHOR: Blaise Barney
C LAST REVISED:  6/10/93
C LAST REVISED:  1/10/94 changed API to MPL   Stacy Pendell
C CONVERTED TO MPI: 11/12/94 by     Xianneng Shen
C **************************************************************************

      program example1_master
      include 'mpif.h'
      integer status(MPI_STATUS_SIZE)
      integer   ARRAYSIZE
      parameter (ARRAYSIZE = 60000)
      parameter (MASTER = 0)

      integer  numtask, numworkers, taskid, dest, index, i,
     &         arraymsg, indexmsg, source, chunksize,
     &         int4, real4
      real*4   data(ARRAYSIZE), result(ARRAYSIZE)

C ************************ initializations ***********************************
C Find out how many tasks are in this partition and what my task id is.  Then
C define the number of worker tasks and the array partition size as chunksize.
C Note:  For this example, the MP_PROCS environment variable should be set
C to an odd number...to insure even distribution of the array to numtasks-1
C worker tasks.
C *****************************************************************************
      call mpi_init(ierr)
      call mpi_comm_rank(MPI_COMM_WORLD, taskid, ierr)
      call mpi_comm_size(MPI_COMM_WORLD, numtasks, ierr)
      write(*,*)'taskid =',taskid
      numworkers = numtasks-1
      chunksize = (ARRAYSIZE / numworkers)
      arraymsg = 1
      indexmsg = 2
      int4 = 4
      real4 = 4

C     Receive my portion of array from the master task */
      call mpi_recv(index, 1, MPI_INTEGER, MASTER, 0, 
     .     MPI_COMM_WORLD, status, ierr)
      call mpi_recv(result(index), chunksize, MPI_REAL, MASTER, 0, 
     .     MPI_COMM_WORLD, status, ierr)

C     Do a simple value assignment to each of my array elements
      do 50 i=index, index + chunksize
        result(i) = i + 1
 50   continue

C     Send my results back to the master
      call mpi_send(index, 1, MPI_INTEGER, MASTER, 1,
     .     MPI_COMM_WORLD, ierr)
      call mpi_send(result(index), chunksize, MPI_REAL, MASTER, 1,
     .     MPI_COMM_WORLD, ierr)

      call mpi_finalize(ierr)
      end

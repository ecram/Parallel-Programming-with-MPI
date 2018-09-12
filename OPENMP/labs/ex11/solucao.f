c
c "loop.F" - Example of loop parallelization
c Copyright(C) 2002, SHARCNet, Canada.
c Ge Baolai <bge@sharcnet.ca>
c
c $Log: loop.F,v $
c Revision 1.1  2002/09/16 21:50:10  bge
c Initial revision
c
      program loop77
      implicit none
      integer i, n/20/
      integer nt/2/, id
      integer omp_get_thread_num
c
      read *, n, nt
c
      call omp_set_num_threads(nt)
c
!$omp parallel do schedule(static, 2)
      do 10 i = 1, n
        id = omp_get_thread_num()
        print *, "Processing ", i, " by thread ", id
   10 continue
!$omp end parallel do
c
      print *
c
!$omp parallel
!$omp do
      do 20 i = 1, n
        id = omp_get_thread_num()
        print *, "Processing ", i, " by thread ", id
   20 continue
!$omp end do nowait
c
!$omp do
      do 30 i = 1, n
        id = omp_get_thread_num()
        print *, "Processing ", -i, " by thread ", id
   30 continue
!$omp end do nowait
!$omp end parallel
      stop
      end

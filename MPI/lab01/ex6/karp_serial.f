       program karp
c
c This simple program approximates pi by computing pi = integral
c from 0 to 1 of 4/(1+x*x)dx which is approximated by sum from
c k=1 to N of 4 / ((1 + (k-1/2)**2 ).  The only input data required is N.
c
c NOTE: Comments that begin with "cspmd" are hints for part b of the
c       lab exercise, where you convert this into an MPI program.
c
cspmd  Each process could be given a chunk of the interval to do.
c
       real err, f, pi, sum, w
       integer i, N
       f(x) = 4.0/(1.0+x*x)
       pi = 4.0*atan(1.0)
cspmd  call startup routine that returns the number of tasks and the
cspmd  taskid of the current instance.

c 
c Now solicit a new value for N.  When it is 0, then you should depart.
       N=1000
       if (N .le. 0) then
          call exit
       endif
       w = 1.0/N
       sum = 0.0
       do i = 1,N
	  sum = sum + f((i-0.5)*w)
       enddo
       sum = sum * w
       err = sum - pi
       print *, 'sum, err =', sum, err
       end

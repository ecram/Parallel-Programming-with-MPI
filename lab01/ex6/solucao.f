       program karp
c karp.soln.f
c This simple program approximates pi by computing pi = integral
c from 0 to 1 of 4/(1+x*x)dx which is approximated by sum from
c k=1 to N of 4 / ((1 + (k-1/2)**2 ).  The only input data required is N.
c
c MPI Parallel version 1 RLF 10/11/95  
c Uses only the 6 basic MPI calls
c
c
       real err, f, pi, sum, w
       include 'mpif.h'
       integer i, N, mynum, nprocs
       integer status(MPI_STATUS_SIZE), mpierr, tag
       f(x) = 4.0/(1.0+x*x)
       pi = 4.0*atan(1.0)

       tag = 111

c All processes call the startup routine to get their rank (mynum)
       call MPI_INIT(mpierr)
       call MPI_COMM_SIZE(MPI_COMM_WORLD,nprocs,mpierr)
       call MPI_COMM_RANK(MPI_COMM_WORLD,mynum,mpierr)

c -------  Each new approximation to pi begins here. -------------------
c (Step 1) Get value N for a new run
5      call solicit (N,nprocs,mynum)

c Step (2): check for exit condition.
       if (N .le. 0) then
          call MPI_FINALIZE(mpierr)
	  call exit
       endif

c Step (3): do the computation in N steps
c Parallel Version: there are "nprocs" processes participating.  Each
c process should do 1/nprocs of the calculation.  Since we want
c i = 1..n but mynum = 0, 1, 2..., we start off with mynum+1.
 
       w = 1.0/N
       sum = 0.0
       do i = mynum+1,N,nprocs
	  sum = sum + f((i-0.5)*w)
       enddo
       sum = sum * w

c Step (4): print the results  
c (Parallel version: collect partial results and let master process print it)
       if (mynum.eq.0) then
	  print *,'host calculated sum=',sum
 	  do i = 1,nprocs-1
 	     call MPI_RECV(x,1,MPI_REAL,i,tag,
     .          MPI_COMM_WORLD,status,mpierr)
             print *,'host got sum=',sum
 	     sum=sum+x
 	  enddo
          err = sum - pi
          print *, 'sum, err =', sum, err
c Other processes just send their sum and go back to 5 and wait for more input
       else
 	  call MPI_SEND(sum,1,MPI_REAL,0,tag,
     .        MPI_COMM_WORLD,mpierr)
       endif
       go to 5
       end


       subroutine solicit (N,nprocs,mynum)
c Get a value for N, the number of intervals in the approximation
c (Parallel versions: master process reads in N and then
c sends N to all the other processes)
c Note: A single broadcast operation could be used instead, but
c is not one of the 6 basics calls.
       include 'mpif.h'
       integer status(MPI_STATUS_SIZE), tag
       tag = 112
       if (mynum .eq. 0) then
          print *,'Enter number of approximation intervals:(0 to exit)'
          read *, N
          do i = 1, nprocs-1
             call MPI_SEND(N,1,MPI_INTEGER,i,tag,
     .            MPI_COMM_WORLD,mpierr)
          enddo
       else
          CALL MPI_RECV(N,1,MPI_INTEGER,0,tag,
     .       MPI_COMM_WORLD,status,mpierr)
       endif
       return
       end

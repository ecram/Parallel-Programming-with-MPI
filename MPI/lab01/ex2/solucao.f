C Program hello.ex1.f
C Parallel version using MPI calls
C Modified from basic version so that workers send back
C a message to the master, who prints out a message
C for each worker
C RLF 10/11/95
	program hello
	include 'mpif.h'
	integer me,nt,mpierr,tag,status(MPI_STATUS_SIZE)
	integer rank
	character(12) message, inmsg

	call MPI_INIT(mpierr)
	call MPI_COMM_SIZE(MPI_COMM_WORLD,nt,mpierr)
	call MPI_COMM_RANK(MPI_COMM_WORLD,me,mpierr)
        tag = 100

	if(me .eq. 0) then
	  message = 'Hello, world'
	  do i=1,nt-1
             call MPI_SEND(message,12,MPI_CHARACTER,i,tag,
     .       MPI_COMM_WORLD,mpierr)
          enddo
          write(6,*)'node',me,':',message
          do i=1,nt-1
             call MPI_RECV(rank,1,MPI_INTEGER,MPI_ANY_SOURCE,tag,
     .       MPI_COMM_WORLD,status,mpierr)
             write(6,*) 'node', rank, ':Hello, back'
          enddo
       else
	  call MPI_RECV(inmsg,12,MPI_CHARACTER,0,tag,
     .      MPI_COMM_WORLD,status,mpierr)
          call MPI_SEND(me,1,MPI_INTEGER,0,tag,
     .       MPI_COMM_WORLD,mpierr)
       endif
       CALL MPI_FINALIZE(mpierr)
       end 


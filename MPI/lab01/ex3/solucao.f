C Program hello.ex2.f
C Parallel version using MPI calls.
C Modified from basic version so that workers send back a message to the 
C master, who prints out a message for each worker.  In addition, the 
C master now sends out two messages to each worker, with two different 
C tags, and the worker receives the messages in reverse order.  Each worker 
C returns a message to the master with a tag equal to the worker's rank, 
C and the master receives that message using the tag.
C RLF 10/11/95

	program hello
	include 'mpif.h'
	integer me,nt,mpierr,tag,status(MPI_STATUS_SIZE)
	integer rank, tag2
	character(6) message1, message2, inmsg1, inmsg2

	call MPI_INIT(mpierr)
	call MPI_COMM_SIZE(MPI_COMM_WORLD,nt,mpierr)
	call MPI_COMM_RANK(MPI_COMM_WORLD,me,mpierr)
        tag = 100
        tag2 = 200

	if(me .eq. 0) then
	  message1 = 'Hello,'
          message2 = 'world'
	  do i=1,nt-1
             call MPI_SEND(message1,6,MPI_CHARACTER,i,tag,
     .       MPI_COMM_WORLD,mpierr)
             call MPI_SEND(message2,6,MPI_CHARACTER,i,tag2,
     .       MPI_COMM_WORLD,mpierr)
          enddo
          write(6,*)'node',me,':',message1,' ',message2
       else
	  call MPI_RECV(inmsg1,6,MPI_CHARACTER,0,tag2,
     .      MPI_COMM_WORLD,status,mpierr)
	  call MPI_RECV(inmsg2,6,MPI_CHARACTER,0,tag,
     .      MPI_COMM_WORLD,status,mpierr)
            write(6,*) 'node', me, ':', inmsg1,' ',inmsg2         
       endif
       CALL MPI_FINALIZE(mpierr)
       end 


C Program hello.ex2.f
C Parallel version using MPI calls.
C Modified from basic version so that workers send back a message to the 
C master, who prints out a message for each worker.  In addition, the 
C master now sends out two messages to each worker, with two different 
C tags, and the worker receives the messages in reverse order.
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
 ===>       (ROTINA MPI)
 ===>       (ROTINA MPI)
          enddo
          write(6,*)'node',me,':',message1,' ',message2
       else
 ===>       (ROTINA MPI)
 ===>       (ROTINA MPI)      
            write(6,*) 'node', me, ':', inmsg1,' ',inmsg2         
       endif
       CALL MPI_FINALIZE(mpierr)
       end 


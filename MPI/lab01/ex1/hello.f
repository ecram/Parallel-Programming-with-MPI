	program hello
	include 'mpif.h'
	integer me,nt,mpierr,tag,status(MPI_STATUS_SIZE)
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
       else
	  call MPI_RECV(inmsg,12,MPI_CHARACTER,0,tag,
     .      MPI_COMM_WORLD,status,mpierr)
	  write(6,*)'node',me,':',inmsg
       endif
       call MPI_FINALIZE(mpierr)
       end 


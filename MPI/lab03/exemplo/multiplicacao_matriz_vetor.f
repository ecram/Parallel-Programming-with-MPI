c
c
c********************************************************************************
c
c                        Example 18 mv_mult_blkstp.f
c
c  Objective            : Matrix Vector Multiplication using block-striped 
c                         partitioning
c	
c  Input                : Real square Matrix and the real vector - Input_A and 
c                         Vector_B .Read files (mdata.inp) for Matrix A and 
c                         (vdata.inp) for Vector b
c
c  Description          : Input matrix is stored in n by n format.
c                         Rowwise block striped partitioning matrix is used. 
c
c  Output               : The final matrix-vector product is stored in a 
c                         array on process with Rank 0 
c
c  Necessary conditions : Number of Processes should be less than or equal 
c                         to 8; Input Matrix Should be Square Matrix. Matrix 
c                         size for Matrix A and vector size for vector b 
c                         should be equally striped, that is Matrix size and 
c                         Vector Size should be divisible by the number of 
c                         processes used.
c
c*******************************************************************************
c
c
c
	 program main

	 include 'mpif.h'

	 parameter (ROW_SIZE_A = 100, COL_SIZE_A = 100)
	 parameter (ROW_SIZE_B = COL_SIZE_A)
	 parameter (TOTAL_SIZE_A = ROW_SIZE_A*COL_SIZE_A)

c 	 ......Variables Initialisation ......
  	 integer n_size, NoofRows_Bloc, NoofRows, NoofCols
        integer NoofVectRows
  	 integer Numprocs, MyRank, Root
  	 integer irow, icol, index

  	 double precision Matrix_A(ROW_SIZE_A, COL_SIZE_A), 
     $	    Input_A(TOTAL_SIZE_A), My_Matrix(ROW_SIZE_A),
     $     Input_Vector(ROW_SIZE_B),
     $     MyOutput_Vector(ROW_SIZE_B),Output_Vector(ROW_SIZE_B)
	 double precision sum

c	 ........MPI Initialisation .......
  	 call MPI_INIT(ierror) 
  	 call MPI_COMM_RANK(MPI_COMM_WORLD, MyRank, ierror)
  	 call MPI_COMM_SIZE(MPI_COMM_WORLD, Numprocs, ierror)
  
c	 .......Read the Matrix From Input file ......
	 Root = 0
	 if(MyRank .eq. Root) then
	   open(unit=12, file = './data/mdata.inp')
           read(12,*) NoofRows, NoofCols
           write(6,*) NoofRows, NoofCols

           do i = 1, NoofRows
              read(12,*) (Matrix_A(i,j), j = 1,NoofCols)
              write(6, 75) (Matrix_A(i,j),j = 1,NoofCols)
           enddo
75	    format(8(2x,f8.3)/)
c
c    ....... Read vector from input file .......
	   open(unit=13, file = './data/vdata.inp')
          read(13,*) NoofVectRows
          read(13,*) (Input_Vector(i), i = 1, NoofVectRows)
          write(6, 75) (Input_Vector(i),i = 1, NoofVectRows)
	   close(12)
	   close(13)

c	   ..... Convert Matrix_A into 1-D array Input_A ...
	  index = 1
	  do irow=1, NoofRows
	     do icol=1, NoofCols
		Input_A(index) = Matrix_A(irow, icol)
		index = index + 1
	     enddo
	  enddo
c
	endif

c   .... Broadcast the number of rows and columns of the matrix to all 
	call MPI_BCAST(NoofRows, 1, MPI_INTEGER, Root, 
     $	               MPI_COMM_WORLD, ierror) 
  	call MPI_BCAST(NoofCols, 1, MPI_INTEGER, Root, 
     $		       MPI_COMM_WORLD, ierror) 
  	call MPI_BCAST(NoofVectRows, 1, MPI_INTEGER, Root, 
     $		       MPI_COMM_WORLD, ierror) 

  	if(NoofRows .ne. NoofCols) then
	   if(MyRank .eq. Root) 
     $	     print*,"Input Matrix is not Square Matrix - Exist."
	   goto 100
	endif

  	if(NoofCols .ne. NoofVectRows) then
	   if(MyRank .eq. Root) 
     $	     print*," Matrix can not be multiplied with vector - Exist"
	   goto 100
	endif

c        .... Size of the square matrix available to all process 
     	n_size = NoofRows
  	if(mod(n_size, Numprocs) .ne. 0) then
	   if(MyRank .eq. 0) 
     $	      print*,"Matrix Can not be Striped Evenly - exist..."
	   goto 100
	endif

  	NoofRows_Bloc = n_size/Numprocs

c  ......Scatter the Input Data (Matrix) stored in the form 
c        of one dimensional array to all process
c
  	call MPI_SCATTER (Input_A, NoofRows_Bloc * n_size, 
     $	     MPI_DOUBLE_PRECISION, My_Matrix, NoofRows_Bloc*n_size, 
     $	     MPI_DOUBLE_PRECISION, Root, MPI_COMM_WORLD,ierror)

  	call MPI_BCAST(Input_Vector, NoofVectRows, MPI_DOUBLE_PRECISION, Root, 
     $	     MPI_COMM_WORLD, ierror) 
c
c    Perform local rowwise matrix-vector multipication on each row
c    and accumlate partial vector values 
c
      	   do irow = 1, NoofRows_Bloc
	      index = (irow-1) * NoofCols 

	      sum = 0.0
	      do icol = 1, NoofCols 
	 	 sum = sum + Input_Vector(icol)*My_Matrix(index+icol)
	      enddo

	      MyOutput_Vector(irow) =  sum
	   enddo

c     ........Gather output vector on the processor 0 
  	   call MPI_GATHER(MyOutput_Vector, NoofRows_Bloc, 
     $		MPI_DOUBLE_PRECISION, Output_Vector, NoofRows_Bloc, 
     $		MPI_DOUBLE_PRECISION, Root, MPI_COMM_WORLD, ierror)

c	.......Output vector .....
	if (MyRank .eq. Root) then
	   write(6, 75) (Output_Vector(irow), irow=1, n_size)
	endif
  
100    call MPI_FINALIZE(ierror)

	stop
	end
c
c	********************************************************
c


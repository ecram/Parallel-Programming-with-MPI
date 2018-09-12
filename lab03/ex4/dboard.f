C *****************************************************************************
C FILE: dboard.f
C DESCRIPTION:
C   Used in pi calculation example codes.
C   See mpi_pi_send.f and mpi_pi_reduce.f
C   Throw darts at board.  Done by generating random numbers
C   between 0 and 1 and converting them to values for x and y
C   coordinates and then testing to see if they "land" in
C   the circle."  If so, score is incremented.  After throwing the
C   specified number of darts, pi is calculated.  The computed value
C   of pi is returned as the value of this function, dboard.
C   Note: Requires Fortran90 compiler due to random_number() function
C AUTHOR: unknown
C LAST REVISED: 01/23/09 Blaise Barney
C **************************************************************************/
C 
C Explanation of constants and variables used in this function:
C   darts    	= number of throws at dartboard
C   score	= number of darts that hit circle
C   n		= index variable
C   r  		= random number between 0 and 1 
C   x_coord	= x coordinate, between -1 and 1  
C   x_sqr	= square of x coordinate
C   y_coord	= y coordinate, between -1 and 1  
C   y_sqr	= square of y coordinate
C   pi		= computed value of pi

      real*8    function dboard(darts)
      integer   darts, score, n
      real*4	r
      real*8	x_coord, x_sqr, y_coord, y_sqr, pi

      score = 0

C     "throw darts at the board"
      do 10 n = 1, darts
C     generate random numbers for x and y coordinates
        call random_number(r)
        x_coord = (2.0 * r) - 1.0
	x_sqr = x_coord * x_coord

        call random_number(r)
        y_coord = (2.0 * r) - 1.0
	y_sqr = y_coord * y_coord

C       if dart lands in circle, increment score
	if ((x_sqr + y_sqr) .le. 1.0) then
	  score = score + 1
        endif

 10   continue

C     calculate pi
      pi = 4.0 * score / darts
      dboard = pi
      end

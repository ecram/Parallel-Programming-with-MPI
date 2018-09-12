C******************************************************************************
C FILE: omp_workshare1.f
C DESCRIPTION:
C   OpenMP Example - Loop Work-sharing - Fortran Version
C   In this example, the iterations of a loop are scheduled dynamically
C   across the team of threads.  A thread will perform CHUNK iterations
C   at a time before being scheduled for the next CHUNK of work.
C AUTHOR: Blaise Barney  5/99
C LAST REVISED: 01/09/04
C******************************************************************************
 
      PROGRAM WORKSHARE1

      INTEGER NTHREADS, TID, OMP_GET_NUM_THREADS,
     +  OMP_GET_THREAD_NUM, N, CHUNKSIZE, CHUNK, I
      PARAMETER (N=100)
      PARAMETER (CHUNKSIZE=10) 
      REAL A(N), B(N), C(N)

!     Some initializations
      DO I = 1, N
        A(I) = I * 1.0
        B(I) = A(I)
      ENDDO
      CHUNK = CHUNKSIZE

!*** Inicio da regiao paralela. Variáveis compartilhadas:A,B,C,NTHREADS,CHUNK ***
!                               Variaveis privadas:I,TID ***

===> Diretiva Openmp


      TID = OMP_GET_THREAD_NUM()
      IF (TID .EQ. 0) THEN
        NTHREADS = OMP_GET_NUM_THREADS()
        PRINT *, 'Number of threads =', NTHREADS
      END IF
      PRINT *, 'Thread',TID,' starting...'

!*** Inicio do loop paralelo, dinamico de tamanho CHUNK ***

===> Diretiva Openmp

      DO I = 1, N
        C(I) = A(I) + B(I)
        WRITE(*,100) TID,I,C(I)
 100    FORMAT(' Thread',I2,': C(',I3,')=',F8.2)
      ENDDO

!*** Fim do loop paralelo sem sincronizacao das threads ***

===> Diretiva Openmp

      PRINT *, 'Thread',TID,' done.'

!*** Fim da regiao paralela

===> Diretiva Openmp

      END



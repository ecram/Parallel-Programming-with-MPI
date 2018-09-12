C******************************************************************************
C OpenMP Example - Combined Parallel Loop Work-sharing - Fortran Version
C FILE: omp_workshare3.f
C DESCRIPTION:
C   This example attempts to show use of the PARALLEL DO construct.  However
C   it will generate errors at compile time.  Try to determine what is causing
C   the error.  See omp_workshare4.f for a corrected version.
C SOURCE: Blaise Barney  5/99
C LAST REVISED: 
C******************************************************************************

      PROGRAM WORKSHARE3

      INTEGER TID, OMP_GET_THREAD_NUM, N, I, CHUNK
      PARAMETER (N=50)
      PARAMETER (CHUNK=5) 
      REAL A(N), B(N), C(N)

!     Some initializations
      DO I = 1, N
        A(I) = I * 1.0
        B(I) = A(I)
      ENDDO
            
!$OMP  PARALLEL DO SHARED(A,B,C,N) 
!$OMP& PRIVATE(I,TID) 
!$OMP& SCHEDULE(STATIC,CHUNK)

      TID = OMP_GET_THREAD_NUM()
      DO I = 1, N
         C(I) = A(I) + B(I)
         PRINT *,'TID= ',TID,'I= ',I,'C(I)= ',C(I)
      ENDDO

!$OMP  END PARALLEL DO

      END

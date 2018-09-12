C******************************************************************************
C OpenMP Example - Matrix-vector multiplication - Fortran Version
C FILE: omp_matvec.f
C DESCRIPTION:
C   This example multiplies all column I elements of matrix A with vector
C   element B(I) and stores the summed products in vector C(I).  A total is
C   maintained for the entire matrix.  Performed by using the OpenMP loop
C   work-sharing construct.  The update of the shared global total is
C   serialized by using the OpenMP critical directive.
C SOURCE: Blaise Barney  5/99
C LAST REVISED:
C******************************************************************************

      PROGRAM MATVEC

      INTEGER I, J, TID, SIZE, OMP_GET_THREAD_NUM
      PARAMETER (SIZE=10)
      REAL A(SIZE,SIZE), B(SIZE), C(SIZE), TOTAL

!     Initializations
      DO I = 1, SIZE
        DO J = 1, SIZE
          A(J,I) = J * 1.0
        ENDDO
        B(I) = I
        C(I) = 0.0
      ENDDO
      PRINT *, 'Starting values of matrix A and vector B'
      DO I = 1, SIZE
        WRITE(*,10) I
  10    FORMAT('A(',I2,')=',$)
        DO J = 1, SIZE
          WRITE(*,20) A(I,J)
  20      FORMAT(F6.2,$)
        ENDDO
        WRITE(*,30) I,B(I)
  30    FORMAT('  B(',I2,')=',F6.2)
        ENDDO
      PRINT *, ' '
      PRINT *, 'Results by thread/column: '

!     Create a team of threads and scope variables
!$OMP PARALLEL SHARED(A,B,C,TOTAL) PRIVATE(I,TID)
      TID = OMP_GET_THREAD_NUM()

!     Loop work-sharing construct - distribute columns of matrix
!$OMP DO PRIVATE(J)
      DO I = 1, SIZE
        DO J = 1, SIZE
          C(I) = C(I) + (A(J,I) * B(I))
        ENDDO

!$OMP CRITICAL
      TOTAL = TOTAL + C(I)
      WRITE(*,40) TID,I,I,C(I),TOTAL
  40  FORMAT('thread',I2,' did column ',I2,'  C(',I2,')= ',F6.2,
     +  '  Running total= ',F8.2)

!$OMP END CRITICAL

      ENDDO
!$OMP END DO

!$OMP END PARALLEL

      PRINT *, ' '
      PRINT *,'Matrix-vector total - sum of all C() = ',TOTAL

      END

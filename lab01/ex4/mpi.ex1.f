C *****************************************************************************
C FILE: mpl.ex1.f
C DESCRIPTION:
C   In this simple example, the master task initiates numtasks-1 number of
C   worker tasks.  It then distributes an equal portion of an array to each
C   worker task.  Each worker task receives its portion of the array, and
C   performs a simple value assignment to each of its elements. The value
C   assigned to each element is simply that element's index in the array+1.
C   Each worker task then sends its portion of the array back to the master
C   task.  As the master receives back each portion of the array, selected
C   elements are displayed.
C AUTHOR: Blaise Barney
C LAST REVISED:  6/10/93
C LAST REVISED:  1/10/94 changed API to MPL   Stacy Pendell
C CONVERTED TO MPI: 11/12/94 by     Xianneng Shen
C **************************************************************************

      program example1_master

====>    INCLUDE MPI

      integer status(MPI_STATUS_SIZE)
      integer   ARRAYSIZE
      parameter (ARRAYSIZE = 60000)
      parameter (MASTER = 0)

      integer  numtask, numworkers, taskid, dest, index, i,
     &         arraymsg, indexmsg, source, chunksize,
     &         int4, real4
      real*4   data(ARRAYSIZE), result(ARRAYSIZE)

C ************************ initializations ***********************************
C Find out how many tasks are in this partition and what my task id is.  Then
C define the number of worker tasks and the array partition size as chunksize.
C Note:  For this example, the MP_PROCS environment variable should be set
C to an odd number...to insure even distribution of the array to numtasks-1
C worker tasks.
C *****************************************************************************

====>    ROTINA MPI DE INICIALIZACAO (ierr)
====>    ROTINA MPI DE IDENTIFICACAORLD (taskid, ierr)
====>    ROTINA MPI PARA DETERMINAR O NUMERO DE PROCESSOS (numtasks, ierr)

      write(*,*)'taskid =',taskid
      numworkers = numtasks-1
      chunksize = (ARRAYSIZE / numworkers)
      arraymsg = 1
      indexmsg = 2
      int4 = 4
      real4 = 4

C *************************** master task *************************************
      if (taskid .eq. MASTER) then
      print *, '*********** Starting MPI Example 1 ************'

C     Initialize the array
      do 20 i=1, ARRAYSIZE 
        data(i) =  0.0
 20   continue

C     Send each worker task its portion of the array
      index = 1
      do 30 dest=1, numworkers
      write(*,*) 'Sending to worker task', dest
      
====>    ROTINA MPI DE ENVIO DE DADOS (index,dest,ierr)
====>    ROTINA MPI DE ENVIO DE DADOS (data(index),chunksize,dest,ierr)

      index = index + chunksize
 30   continue

C     Now wait to receive back the results from each worker task and print 
C     a few sample values 
      do 40 i=1, numworkers
        source = i

====>    ROTINA MPI PARA RECEBER DADOS (index,source,status,ierr)
====>    ROTINA MPI PARA RECEBER DADOS (result(index),chunksize,source,status,ierr)

        print *, '---------------------------------------------------'
        print *, 'MASTER: Sample results from worker task ', source
        print *, '   result[', index, ']=', result(index)
        print *, '   result[', index+100, ']=', result(index+100)
        print *, '   result[', index+1000, ']=', result(index+1000)
        print *, ' '
 40   continue

      print *, 'MASTER: All Done!' 
      endif

C *************************** worker task ************************************
      if (taskid .gt. MASTER) then
C     Receive my portion of array from the master task */

====>    ROTINA MPI PARA RECEBER DADOS (index,MASTER,status,ierr)
====>    ROTINA MPI PARA RECEBER DADOS (result(index),chunksize,MASTER,status,ierr)


C     Do a simple value assignment to each of my array elements
      do 50 i=index, index + chunksize
        result(i) = i + 1
 50   continue

C     Send my results back to the master

====>    ROTINA MPI DE ENVIO DE DADOS (index,MASTER,ierr)
====>    ROTINA MPI DE ENVIO DE DADOS (result(index),chunksize,MASTER,ierr)

      endif
      
====>    ROTINA MPI DE FINALIZACAO (ierr)
 
      end

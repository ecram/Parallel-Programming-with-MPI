/*
 * "loop.c" - Example of loop parallelization
 * Copyright(C) 2002, SHARCNet, Canada.
 * Ge Baolai <bge@sharcnet.ca>
 *
 * $Log: loop.c,v $
 * Revision 1.1  2002/09/16 21:49:59  bge
 * Initial revision
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include "omp.h"

int main( int argc, char *argv[] )
{
    int i, n = 20;
    int nt = 2, id;

    /*
     * Get loop size and number of threads from command line
     */
    if (argc >= 2) n = atoi(argv[1]);
    if (argc == 3) nt = atoi(argv[2]);

    /*
     * Set number of threads
     */
    omp_set_num_threads(nt);

    /*
     * The loop is parallelized
     */
    #pragma omp parallel for schedule(static, 2)
    for (i = 0; i < n; i++)
    {
        id = omp_get_thread_num();
        printf("Processing %d by thread %d\n", i+1, id);
    }
    
    printf("\n");

    /*
     * The following two loops are executed completely in parallel
     */
    #pragma omp parallel
    {
        #pragma omp for schedule(static, 2) nowait
        for (i = 0; i < n; i++)
        {
            id = omp_get_thread_num();
            printf("Processing %d by thread %d\n", i+1, id);
        }
        #pragma omp for schedule(static, 2) nowait
        for (i = 0; i < n; i++)
        {
            id = omp_get_thread_num();
            printf("Processing %d by thread %d\n", -(i+1), id);
        }
    }

    return 0;
}

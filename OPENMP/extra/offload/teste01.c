#include <stdio.h>
#include <omp.h>


int main(){
	#pragma omp parallel 
	{
	if( omp_get_thread_num() == 0 )
		printf("threads=%d\n",omp_get_num_threads());
	}
	#pragma omp target
	#pragma omp parallel 
	{
	if( omp_get_thread_num() == 0 )
		printf("threads=%d\n",omp_get_num_threads());
	}
	return 0;
}

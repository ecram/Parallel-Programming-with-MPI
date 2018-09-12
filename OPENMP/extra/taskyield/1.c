#include <unistd.h>
#include <stdio.h>
#include <omp.h>

int main(){
	int i,p;
	omp_lock_t my_lock;
	#pragma omp parallel for default(none) private(i) shared(p,my_lock)
	for( i=0 ; i<1000 ; i++ ){
		omp_set_lock(&my_lock);
		printf("Thread %d init %d\n",omp_get_thread_num(),i);
		p+=i;
		sleep(i%5);
		omp_unset_lock(&my_lock);
		printf("Thread %d done %d\n",omp_get_thread_num(),i);
	}
	return 0;
}

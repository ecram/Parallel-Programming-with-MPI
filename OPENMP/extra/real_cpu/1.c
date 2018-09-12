#include <stdio.h>
#include <omp.h>
#include <unistd.h>
#include <sys/syscall.h>


static inline int getcpu() {
    #ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
    #else
    return -1; // unavailable
    #endif
}

int main(){
	#pragma omp parallel 
	//printf("ThreadID=%d, cpu_id=%d\n",omp_get_thread_num(),sched_getcpu());
	printf("ThreadID=%d, cpu_id=%d\n",omp_get_thread_num(),getcpu());
	return 0;
}

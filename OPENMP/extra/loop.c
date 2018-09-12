#include <stdio.h>
#include <omp.h>

int main(){
	int i,x;
	#pragma omp parallel for private(i)
	for(i=0 ;i<=10 ; i++);
	for(i=0,x=10 ;i<=10 ; i++);
}

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>


float foo(float *A, int n){
	int i=0;
	float x = 0;
	#pragma GCC ivdep
	for (i=0; i<n; i++){
		x+=A[i];
	}
	return x;
}
#define SIZE 1000
int main(){
	float *x=malloc(sizeof(float)*SIZE);
	int cont;
	#pragma GCC ivdep
	for( cont=0;cont<SIZE;cont++) x[cont]=cont*cont;
	printf("%f\n",foo(x,SIZE));
	free(x); x=NULL;
	return 0;
}

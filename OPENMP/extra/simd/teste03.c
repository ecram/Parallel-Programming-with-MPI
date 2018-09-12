#include <stdio.h>
#include <stdlib.h>
#include <omp.h>


long double foo(long double *A, long long int n){
	long long int i=0;
	long double x = 0;
	#pragma omp parallel for reduction(+:x) 
	for (i=0; i<n; i++){
		x = x + A[i];
	}
	return x;
}
#define SIZE 10000000
int main(){
	long double *x=malloc(sizeof(long double)*SIZE);
	long long int cont;
	for( cont=0;cont<SIZE;cont++) x[cont]=cont*cont;
	printf("%Lf\n",foo(x,SIZE));
	free(x); x=NULL;
	return 0;
}

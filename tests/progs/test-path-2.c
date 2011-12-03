/**
 * Test the path_may_write function. 
 * The function shouldn't trace into whatever callee functions. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

volatile int *a = NULL;

__attribute__((noinline)) void foo() {
	a[1] = a[0];
	printf("self = %lu\n", pthread_self());
	a[0] = 2;
}

int main() {
	a = (int *)malloc(sizeof(int) * 2);
	a[0] = 1;
	a[1] = 0;
	foo();
	printf("a[1] = %d\n", a[1]);
	return 0;
}

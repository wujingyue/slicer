#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int n = 10;

int main(int argc, char *argv[]) {
	
	int i;

	if (argc >= 2)
		n = atoi(argv[1]);
	
	void *a[n];
	for (i = 0; i < n; ++i)
		a[i] = malloc(n * sizeof(int));

	for (i = 0; i < n; ++i)
		fprintf(stderr, "self = %lu\n", pthread_self());
	
	return 0;
}

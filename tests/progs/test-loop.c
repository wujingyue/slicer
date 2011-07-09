#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

volatile int n;

int main(int argc, char *argv[]) {
	
	int i;

	if (argc < 2) {
		fprintf(stderr, "%s <# of iterations>\n", argv[0]);
		exit(1);
	}
	
	n = atoi(argv[1]);
	for (i = 0; i < n; ++i)
		printf("%d\n", i);

	for (i = 0; i < n; ++i)
		fprintf(stderr, "self = %lu\n", pthread_self());
	
	return 0;
}

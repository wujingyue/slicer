#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

volatile int n = 10;

int main(int argc, char *argv[]) {
	int i;
	if (argc > 1)
		n = atoi(argv[1]);
	for (i = 0; i < n; ++i)
		printf("i = %d\n", i);
	for (i = 0; i < n; ++i)
		fprintf(stderr, "self = %lu\n", pthread_self());
	return 0;
}

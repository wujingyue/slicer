#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void foo(int a) {
	if (a == 2)
		fprintf(stderr, "foo: %lu\n", pthread_self());
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <0/1>\n", argv[0]);
		exit(1);
	}
	if (atoi(argv[1]) == 0) {
		foo(0);
		fprintf(stderr, "true: %lu\n", pthread_self());
	} else {
		foo(1);
		fprintf(stderr, "false: %lu\n", pthread_self());
	}
	return 0;
}

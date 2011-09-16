#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void foo() {
	fprintf(stderr, "foo: %lu\n", pthread_self());
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <0/1>\n", argv[0]);
		exit(1);
	}
	if (atoi(argv[1]) == 0) {
		foo();
		fprintf(stderr, "true: %lu\n", pthread_self());
	} else {
		foo();
		fprintf(stderr, "false: %lu\n", pthread_self());
	}
	return 0;
}

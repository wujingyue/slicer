#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void foo() __attribute__((noinline));
void foo() {
	if (rand() % 100 == 0)
		fprintf(stderr, "self = %lu\n", pthread_self());
}

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i < 1000; ++i)
		foo();
	return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

volatile int n = 10;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *foo(void *arg) {
	if (arg != (void *)0)
		printf("Thread %lu\n", pthread_self());
	printf("n = %d\n", n);
	pthread_mutex_lock(&mutex);
	printf("n = %d\n", n);
	pthread_mutex_unlock(&mutex);
	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t child;

	if (argc > 1)
		n = atoi(argv[1]);

	pthread_create(&child, NULL, foo, (void *)1);
	sleep(1);
	foo((void *)0);
	pthread_join(child, NULL);

	return 0;
}

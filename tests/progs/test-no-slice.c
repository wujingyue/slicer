#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *foo(void *arg) {
	long size = (long)arg;
	int *a = (int *)malloc(sizeof(int) * size);
	long i;

	for (i = 0; i < size; ++i)
		a[i] = i;
	free(a);

	return NULL;
}

int main() {
	pthread_t t1, t2;
	pthread_create(&t1, NULL, foo, (void *)1024);
	pthread_create(&t2, NULL, foo, (void *)2048);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	return 0;
}

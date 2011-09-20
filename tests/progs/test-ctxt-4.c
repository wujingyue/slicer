/**
 * Author: Jingyue
 *
 * Test whether the range analysis is context-sensitive. 
 */

#include <stdio.h>
#include <pthread.h>

#define N (4)

volatile int a[N * N];

__attribute__((noinline)) void access(long s, long e) {
	long i;
	for (i = s; i < e; ++i)
		a[i] = i;
}

__attribute__((noinline)) void *worker(void *arg) {
	long i = (long)arg;
	access(i * N, (i + 1) * N);
	return NULL;
}

int main() {
	long i;
	pthread_t children[N];

	for (i = 0; i < N; ++i)
		pthread_create(&children[i], NULL, worker, (void *)i);

	for (i = 0; i < N; ++i)
		pthread_join(children[i], NULL);

	return 0;
}

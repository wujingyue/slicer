#include <stdio.h>
#include <pthread.h>

pthread_barrier_t bar;
volatile int a = 0, b = 0;

void *child1(void *arg) {
	a = 1;
	pthread_barrier_wait(&bar);
	b = 2;
	return NULL;
}

void *child2(void *arg) {
	b = 1;
	pthread_barrier_wait(&bar);
	a = 2;
	return NULL;
}

int main() {
	pthread_t t1, t2;

	pthread_barrier_init(&bar, NULL, 2);
	pthread_create(&t1, NULL, child1, NULL);
	pthread_create(&t2, NULL, child2, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_barrier_destroy(&bar);

	printf("a = %d\n", a);
	printf("b = %d\n", b);
	
	return 0;
}

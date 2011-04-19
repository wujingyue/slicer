#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void process(int *a) {
	++a[0];
}

void *foo(void *arg) {
	int *a = (int *)malloc(sizeof(int));
	a[0] = 0;
	process(a);
	return a;
}

int main() {
	pthread_t t1, t2;
	void *a1, *a2;
	int sum;

	pthread_create(&t1, NULL, foo, NULL);
	pthread_create(&t2, NULL, foo, NULL);
	pthread_join(t1, &a1);
	pthread_join(t2, &a2);
	
	sum += ((int *)a1)[0];
	sum += ((int *)a2)[0];
	printf("sum = %d\n", sum);

	return 0;
}


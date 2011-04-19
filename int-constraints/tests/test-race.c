#include <stdio.h>
#include <pthread.h>

int a = 0;

void *foo(void *arg) {
	++a;
}

int main() {
	pthread_t t1, t2;
	pthread_create(&t1, NULL, foo, NULL);
	pthread_create(&t2, NULL, foo, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	printf("%d\n", a);
	return 0;
}


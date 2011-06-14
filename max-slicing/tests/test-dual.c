#include <stdio.h>
#include <pthread.h>

void *foo(void *arg) {
	printf("hello world.\n");
}

int main() {
	pthread_t t;
	pthread_create(&t, NULL, foo, NULL);
	pthread_join(t, NULL);
	foo(NULL);
	return 0;
}


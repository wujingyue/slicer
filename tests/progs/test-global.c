#include <stdio.h>
#include <pthread.h>

int a = 0;

__attribute__((noinline)) void transpose(long delta) {
	printf("a + delta = %d\n", a + (int)delta);
}

void *foo(void *arg) {
	transpose((long)arg);
	return NULL;
}

int main(int argc, char *argv[]) {
	if (argc > 1)
		return 1;
	a = argc;
	pthread_t t1, t2;
	pthread_create(&t1, NULL, foo, (void *)1);
	pthread_create(&t2, NULL, foo, (void *)2);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	return 0;
}

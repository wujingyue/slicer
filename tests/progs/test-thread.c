#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; ++i) {
		pthread_mutex_lock(&mutex);
		printf("i = %d\n", i);
		pthread_mutex_unlock(&mutex);
	}
	return 0;
}


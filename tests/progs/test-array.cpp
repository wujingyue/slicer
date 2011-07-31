#include <pthread.h>
#include <stdio.h>

#define SIZE (10)
#define N_THREADS (2)

pthread_barrier_t barrier;
int global_arr[SIZE];

void *sub_routine(void *arg) {
	long my_num = (long)arg;

	pthread_barrier_wait(&barrier);
	global_arr[my_num]++;
	pthread_barrier_wait(&barrier);
	
	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t children[N_THREADS];
	long i;

	for (i = 0; i < SIZE; ++i)
		global_arr[i] = 0;

	pthread_barrier_init(&barrier, NULL, N_THREADS); 
	for (i = 0; i < N_THREADS; ++i)
		pthread_create(&children[i], NULL, sub_routine, (void *)i);
	for (i = 0; i < N_THREADS; ++i)
		pthread_join(children[i], NULL);

	return 0;
}

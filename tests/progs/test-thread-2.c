/**
 * Author: Jingyue
 *
 * There used to be a bug where global_arr must be initialized as 0. 
 * After fixing it, the integer constraint solver should be able to run
 * without any problem. 
 */

#include <pthread.h>
#include <stdio.h>

#define SIZE (10)
#define N_THREADS (2)

pthread_barrier_t barrier;
int global_arr[SIZE];

void *sub_routine(void *arg) {
	long my_num = (long)arg;
	long i;

	pthread_barrier_wait(&barrier);
	for (i = 0; i < 5; i++) {
		global_arr[my_num]++;
	}
	pthread_barrier_wait(&barrier);

	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t children[N_THREADS];
	long i;

	for (i = 0; i < SIZE; ++i)
		global_arr[i]++;

	pthread_barrier_init(&barrier, NULL, N_THREADS); 
	for (i = 0; i < N_THREADS; ++i)
		pthread_create(&children[i], NULL, sub_routine, (void *)(i + 1));
	for (i = 0; i < N_THREADS; ++i)
		pthread_join(children[i], NULL);

	return 0;
}

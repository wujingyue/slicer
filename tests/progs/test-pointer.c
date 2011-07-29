#include <pthread.h>
#include <stdio.h>

#define SIZE (10)
#define N_THREADS (2)

pthread_barrier_t barrier;
int global_arr[SIZE];

void *sub_routine(void *arg) {
	long my_num = (long)arg;

	pthread_barrier_wait(&barrier);
	if (my_num != 0)
		global_arr[my_num]++;
	pthread_barrier_wait(&barrier);

	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t t1, t2;
	long i;

	for (i = 0; i < SIZE; ++i)
		global_arr[i] = 0;

	pthread_barrier_init(&barrier, NULL, N_THREADS); 
	pthread_create(&t1, NULL, sub_routine, (void *)1);
	pthread_create(&t2, NULL, sub_routine, (void *)3);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	return 0;
}

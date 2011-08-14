#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

long *rank_me;
long size = 1024;

void *slave_sort(void *arg) {
	long i = (long)arg;
	long j;
	long *rank_me_mynum;

	sleep(1);
	rank_me_mynum = (long *)rank_me[i];
	for (j = 0; j < size; ++j)
		rank_me_mynum[j] = 0;

	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t t1, t2;

	size = atol(argv[1]);
	if (size <= 0)
		exit(-1);
	
	rank_me = malloc(sizeof(long) * (2 + 2 * size));
	rank_me[0] = (long)(rank_me + 2);
	rank_me[1] = (long)(rank_me + 2 + size);
	
	pthread_create(&t1, NULL, slave_sort, (void *)0);
	pthread_create(&t2, NULL, slave_sort, (void *)1);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	free(rank_me);
	return 0;
}

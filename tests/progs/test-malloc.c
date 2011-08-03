#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_P (1024)
#define SIZE (4096)

struct GlobalPrivate {
	int *rank_ff;
} gp[MAX_P];

void *foo0(void *arg) {
	gp[0].rank_ff[0] = 5;
	return NULL;
}

void *foo1(void *arg) {
	gp[1].rank_ff[0] = 5;
	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t t1, t2;

	gp[0].rank_ff = malloc(SIZE);
	gp[1].rank_ff = malloc(SIZE);
	pthread_create(&t1, NULL, foo0, NULL);
	pthread_create(&t2, NULL, foo1, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	return 0;
}

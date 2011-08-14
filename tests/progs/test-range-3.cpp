#include <pthread.h>

#include <cstdio>
#include <cstdlib>
using namespace std;

pthread_barrier_t barrier;
pthread_mutex_t mutex;
int size = -1;
int nThreads = -1;
int N = -1;
int rootN = -1;
double *XXX;

void run(double *x, int xSize, int my, int start)
	__attribute__((always_inline));

void runTwiddle(double *x, int xSize, int my, int start)
	__attribute__((always_inline));

void run(double *x, int xSize, int my, int start) {
	long q, n;
	n = (1 << (size / 2));
	for (q = 1; q <= size / 2; q++) {
		long L, Lstar, j, r, k;
		L = (1 << q);
		Lstar = L / 2;
		r = n / L;
		for (k = 0; k < r; ++k) {
			double *xxx = &x[2 * k * L];
			double *yyy = &x[2 * (k * L + Lstar)];
			for (j = 0; j < Lstar; j++) {
				xxx[2 * j] += (my + 0.1);
				xxx[2 * j + 1] += (my + 0.01);
				yyy[2 * j] += (my + 0.001);
				yyy[2 * j + 1] += (my + 0.0001);
			}
		}
	}
}

void runTwiddle(double *x, int xSize, int my, int start) {
	for (int i = 0; i < xSize; ++i) {
		x[2 * i] += (my + 0.1);
		x[2 * i + 1] += (my + 0.1);
	}
}

void *subRoutine(void *arg) {

	pthread_barrier_wait(&barrier);

	long my = (long)arg;
	int start = my * rootN / nThreads;
	int end = start + rootN / nThreads;
	for (int i = start; i < end; i++) {
		run(&XXX[2 * i * rootN], rootN, my, 2 * i * rootN);
		runTwiddle(&XXX[2 * i * rootN], rootN, my, 2 * i * rootN);
	}
	
	pthread_barrier_wait(&barrier);

	return NULL;
}

int main(int argc, char *argv[]) {

	nThreads = atoi(argv[1]);
	size = atoi(argv[2]);
	if (size < nThreads)
		exit(1);
	if (size > 16)
		exit(1);

	pthread_t *threadIds = (pthread_t *)malloc(sizeof(pthread_t) * nThreads);

	N = (1 << size);
	rootN = (1 << (size / 2));
	XXX = (double *)malloc(sizeof(double) * 2 * (N + rootN));

	for (int i = 0; i < 2 * (N + rootN); i++)
		XXX[i] = 0;

	pthread_barrier_init(&barrier, NULL, nThreads);

	for (int i = 0; i < nThreads - 1; i++)
		pthread_create(&threadIds[0], NULL, subRoutine, (void *)i);

	subRoutine((void *)(nThreads - 1));

	for (int i = 0; i < nThreads - 1; i++)
		pthread_join(threadIds[i], NULL);

	free(threadIds);
	free(XXX);

	return 0;
}

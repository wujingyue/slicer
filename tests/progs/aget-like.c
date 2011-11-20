#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define GETRECVSIZ (8192)
#define MAX_N_THREADS (1024)

struct thread_data {
	long long soffset, foffset, offset;
	pthread_t tid;
};

struct thread_data wthread[MAX_N_THREADS];
int nthreads;
unsigned int bwritten = 0;
pthread_mutex_t bwritten_mutex = PTHREAD_MUTEX_INITIALIZER;

long long fake_read(char *buffer, size_t size) __attribute__((noinline));
long long fake_write(char *buffer, size_t size, size_t offset)
	__attribute__((noinline));

long long fake_read(char *buffer, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i)
		buffer[i] = rand() % 26 + 'a';
	return size;
}

long long fake_write(char *buffer, size_t size, size_t offset) {
	size_t i;
	for (i = 0; i < size; ++i)
		fprintf(stderr, "%d: %c\n", offset + i, buffer[i]);
	return size;
}

void *http_get(struct thread_data *td) {
	long long foffset;
	char *rbuf;
	pthread_t tid;

	tid = pthread_self();

	rbuf = (char *)calloc(GETRECVSIZ, sizeof(char));
	foffset = td->foffset;
	td->offset = td->soffset;
	while (td->offset < foffset) {
		long long dr, dw;

		memset(rbuf, GETRECVSIZ, 0);
		dr = fake_read(rbuf, GETRECVSIZ);
		if (td->offset + dr > foffset)
			dw = fake_write(rbuf, foffset - td->offset, td->offset);
		else
			dw = fake_write(rbuf, dr, td->offset);
		td->offset += dw;
		pthread_mutex_lock(&bwritten_mutex);
		bwritten += dw;
		pthread_mutex_unlock(&bwritten_mutex);
	}
	
	return NULL;
}

long long calc_offset(long long total, int part, int nthreads) {
	return (part * (total / nthreads));
}

int main(int argc, char *argv[]) {
	int i;
	long long clength;

	if (argc < 3) {
		fprintf(stderr, "%s <file length> <# of threads>\n", argv[0]);
		return 1;
	}
	clength = atoi(argv[1]);
	nthreads = atoi(argv[2]);

	for (i = 0; i < nthreads; ++i) {
		long long soffset = calc_offset(clength, i, nthreads);
		long long foffset = calc_offset(clength, i + 1, nthreads);
		wthread[i].soffset = soffset;
		wthread[i].foffset = (i == nthreads - 1 ? clength : foffset);
		pthread_create(&(wthread[i].tid), NULL,
				(void *(*)(void *))http_get, &(wthread[i]));
	}

	for (i = 0; i < nthreads; ++i)
		pthread_join(wthread[i].tid, NULL);

	return 0;
}

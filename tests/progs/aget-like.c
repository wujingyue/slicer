#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define GETRECVSIZ (8192)

struct thread_data {
	long long soffset, foffset, offset;
	pthread_t tid;
};

struct thread_data *wthread;
int nthreads = 1;
unsigned int bwritten = 0;
pthread_mutex_t bwritten_mutex = PTHREAD_MUTEX_INITIALIZER;

void *http_get(void *arg) {
	struct thread_data *td = arg;
	long long foffset = td->foffset;
	char *rbuf = (char *)calloc(GETRECVSIZ, sizeof(char));

	td->offset = td->soffset;
	while (td->offset < foffset) {
		long long dr, dw;

		memset(rbuf, GETRECVSIZ, 0);
		dr = read(STDIN_FILENO, rbuf, GETRECVSIZ);
		if (td->offset + dr > foffset)
			dw = write(STDOUT_FILENO, rbuf, foffset - td->offset);
		else
			dw = write(STDOUT_FILENO, rbuf, dr);
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

	if (argc < 2)
		return 1;
	clength = atoi(argv[1]);
	if (argc > 2)
		nthreads = atoi(argv[2]);

	wthread = (struct thread_data *)malloc(nthreads * sizeof(struct thread_data));

	for (i = 0; i < nthreads; ++i) {
		long long soffset = calc_offset(clength, i, nthreads);
		long long foffset = calc_offset(clength, i + 1, nthreads);
		wthread[i].soffset = soffset;
		wthread[i].foffset = (i == nthreads - 1 ? clength : foffset);
		pthread_create(&(wthread[i].tid), NULL, http_get, &(wthread[i]));
	}

	for (i = 0; i < nthreads; ++i)
		pthread_join(wthread[i].tid, NULL);

	return 0;
}

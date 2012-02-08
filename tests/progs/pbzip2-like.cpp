#include <iostream>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
using namespace std;

const int buf_size = 1024;
const int max_n_bufs = 1024;

struct Queue {
	Queue() {}

	void init() {
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&not_empty, NULL);
		pthread_cond_init(&not_full, NULL);
		head = tail = 0;
		empty = true;
		full = false;
		memset(bufs, 0, sizeof bufs);
	}

	char *del() {
		assert(!empty);
		char *cur = bufs[head];
		head = (head + 1) % max_n_bufs;
		if (head == tail)
			empty = true;
		full = false;
		return cur;
	}

	void add(char *buf) {
		assert(!full);
		bufs[tail] = buf;
		tail = (tail + 1) % max_n_bufs;
		empty = false;
		if (head == tail)
			full = true;
	}

	pthread_mutex_t mutex;
	pthread_cond_t not_empty, not_full;
	bool empty, full;
	int head, tail;
	char *bufs[max_n_bufs];
};

Queue q;

void *producer(void *arg) {
	long n_blocks = (long)arg;
	for (long i = 0; i < n_blocks; ++i) {
		char *file_data = new char[buf_size];
		for (int j = 0; j < buf_size; ++j)
			file_data[j] = rand() % 26 + 'a';
		pthread_mutex_lock(&q.mutex);
		while (q.full) {
			pthread_cond_wait(&q.not_full, &q.mutex);
		}
		q.add(file_data);
		pthread_mutex_unlock(&q.mutex);
		pthread_cond_signal(&q.not_empty);
	}
	return NULL;
}

void *consumer(void *arg) {
	long n_blocks = (long)arg;
	char result = 0;
	for (long i = 0; i < n_blocks; ++i) {
		pthread_mutex_lock(&q.mutex);
		while (q.empty) {
			pthread_cond_wait(&q.not_empty, &q.mutex);
		}
		char *file_data = q.del();
		pthread_mutex_unlock(&q.mutex);
		pthread_cond_signal(&q.not_full);
		for (int j = 0; j < buf_size; ++j) {
			result += file_data[j];
		}
		delete[] file_data;
	}
	printf("result = %d\n", (int)result);
	return NULL;
}

int main(int argc, char *argv[]) {
	assert(argc > 1);
	long n_blocks = atol(argv[1]);

	q.init();
	pthread_t t_producer, t_consumer;
	pthread_create(&t_producer, NULL, producer, (void *)n_blocks);
	pthread_create(&t_consumer, NULL, consumer, (void *)n_blocks);
	pthread_join(t_producer, NULL);
	pthread_join(t_consumer, NULL);
	return 0;
}

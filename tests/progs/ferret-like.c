#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define BUF_SIZE (1024)
#define MAX_N_NODES (1024)

struct Node {
	char buf[BUF_SIZE];
	struct Node *next;
};

struct Queue {
	pthread_mutex_t mutex;
	pthread_cond_t not_empty, not_full;
	int count;
	struct Node *head, *tail;
};

void queue_init(struct Queue *q) {
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->not_empty, NULL);
	pthread_cond_init(&q->not_full, NULL);
	q->count = 0;
	q->head = q->tail = NULL;
}

struct Node *queue_dequeue(struct Queue *q) {
	struct Node *cur;

	assert(q->count > 0);
	--q->count;
	cur = q->head;
	q->head = cur->next;
	cur->next = NULL;
	if (q->count == 0)
		q->tail = NULL;
	return cur;
}

void queue_enqueue(struct Queue *q, struct Node *n) {
	assert(q->count < MAX_N_NODES);
	++q->count;
	if (q->count == 1) {
		q->head = q->tail = n;
	} else {
		q->tail->next = n;
		q->tail = n;
	}
}

struct Queue tasks;

void *producer(void *arg) {
	long n_blocks = (long)arg;
	long i;

	for (i = 0; i < n_blocks; ++i) {
		struct Node *node = (struct Node *)malloc(sizeof(struct Node));
		int j;

		node->next = NULL;
		for (j = 0; j < buf_size; ++j)
			node->buf[j] = rand() % 26 + 'a';

		pthread_mutex_lock(&tasks.mutex);
		while (tasks.count >= MAX_N_NODES)
			pthread_cond_wait(&tasks.not_full, &tasks.mutex);
		queue_enqueue(&tasks, node);
		pthread_mutex_unlock(&tasks.mutex);
		pthread_cond_signal(&tasks.not_empty);
	}

	return NULL;
}

void *consumer(void *arg) {
	long n_blocks = (long)arg;
	char result = 0;
	long i;

	for (i = 0; i < n_blocks; ++i) {
		int j;
		struct Node *node;

		pthread_mutex_lock(&tasks.mutex);
		while (tasks.count == 0)
			pthread_cond_wait(&tasks.not_empty, &tasks.mutex);
		node = queue_dequeue(&tasks);
		pthread_mutex_unlock(&tasks.mutex);
		pthread_cond_signal(&tasks.not_full);
		for (j = 0; j < buf_size; ++j)
			result += node->buf[j];

		free(node);
	}

	printf("result = %d\n", (int)result);

	return NULL;
}

int main(int argc, char *argv[]) {
	long n_blocks;
	pthread_t t_producer, t_consumer;

	assert(argc > 1);
	n_blocks = atol(argv[1]);

	queue_init(&tasks);
	pthread_create(&t_producer, NULL, producer, (void *)n_blocks);
	pthread_create(&t_consumer, NULL, consumer, (void *)n_blocks);
	pthread_join(t_producer, NULL);
	pthread_join(t_consumer, NULL);

	return 0;
}

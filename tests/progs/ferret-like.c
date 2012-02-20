#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

const int buf_size = 1024;
const int max_n_nodes = 1024;

struct Node {
	char buf[buf_size];
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
	if (q->head == NULL)
		q->tail = NULL;
	return cur;
}

void queue_enqueue(struct Queue *q, struct Node *n) {
	assert(q->count < max_n_nodes);
	++q->count;
	if (q->tail == NULL) {
		q->head = q->tail = n;
	} else {
		q->tail->next = n;
		q->tail = n;
	}
}

struct Queue q;

void *producer(void *arg) {
	long n_blocks = (long)arg;
	long i;

	for (i = 0; i < n_blocks; ++i) {
		struct Node *node = (struct Node *)malloc(sizeof(struct Node));
		int j;

		node->next = NULL;
		for (j = 0; j < buf_size; ++j)
			node->buf[j] = rand() % 26 + 'a';

		pthread_mutex_lock(&q.mutex);
		while (q.count >= max_n_nodes)
			pthread_cond_wait(&q.not_full, &q.mutex);
		queue_enqueue(&q, node);
		pthread_mutex_unlock(&q.mutex);
		pthread_cond_signal(&q.not_empty);
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

		pthread_mutex_lock(&q.mutex);
		while (q.count == 0)
			pthread_cond_wait(&q.not_empty, &q.mutex);
		node = queue_dequeue(&q);
		pthread_mutex_unlock(&q.mutex);
		pthread_cond_signal(&q.not_full);
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

	queue_init(&q);
	pthread_create(&t_producer, NULL, producer, (void *)n_blocks);
	pthread_create(&t_consumer, NULL, consumer, (void *)n_blocks);
	pthread_join(t_producer, NULL);
	pthread_join(t_consumer, NULL);

	return 0;
}

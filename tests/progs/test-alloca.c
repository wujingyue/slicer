/**
 * Author: Jingyue
 *
 * Test if the integer analysis is able to distinguish allocas. 
 */

#include <stdio.h>
#include <pthread.h>

struct Pair {
	int first;
	int second;
};

void *worker1(void *arg) {
	struct Pair *p = (struct Pair *)arg;
	p->second = 1;
	return NULL;
}

void *worker2(void *arg) {
	struct Pair *p = (struct Pair *)arg;
	p->first = 1;
	return NULL;
}

int main() {
	struct Pair a = {0, 0}, b = {0, 0};
	pthread_t t1, t2;

	pthread_create(&t1, NULL, worker1, &a);
	pthread_create(&t2, NULL, worker2, &b);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	return 0;
}

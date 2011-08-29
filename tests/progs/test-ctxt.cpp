/**
 * Author: Jingyue
 *
 * Test whether max-slicing is working under complicated calling contexts.
 */

#include <pthread.h>

#include <iostream>
using namespace std;

void *foo(void *arg) {
	printf("Hello world\n");
	return NULL;
}

void bar(long i) {
	if (i > 1)
		foo(NULL);
}

int main() {
	pthread_t child;
	long i;

	pthread_create(&child, NULL, foo, NULL);
	pthread_join(child, NULL);

	for (i = 0; i < 3; ++i)
		bar(i);

	return 0;
}

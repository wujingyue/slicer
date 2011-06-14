#include <iostream>
#include <pthread.h>
using namespace std;

void *foo(void *arg) {
	cerr << "hello world.\n";
}

int main() {
	pthread_t t;
	pthread_create(&t, NULL, foo, NULL);
	pthread_join(t, NULL);
	return 0;
}


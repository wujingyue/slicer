#include <stdio.h>
#include <pthread.h>

int foo(int a) {
	return a;
}

int main() {
	int i, r;

	foo(1);
	foo(2);
	r = foo(3);
	for (i = 0; i < r; ++i)
		printf("%lu\n", pthread_self());
	
	return 0;
}

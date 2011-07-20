#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define max_n (1024)

volatile int arr[max_n];

void foo1(int l, int h) {
	int i;
	for (i = l; i < h; ++i)
		arr[i] = i;
}

void foo2(int l, int h) {
	int i;
	for (i = l; i < h; ++i)
		arr[i] = i;
}

int main(int argc, char *argv[]) {

	int a, b, c;

	assert(argc >= 4);
	a = atoi(argv[1]);
	b = atoi(argv[2]);
	c = atoi(argv[3]);
	assert(a < b && b < c);

	foo1(a, b);
	foo2(b, c);

	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
	int i = 0, r = 0;

	assert(argc > 2);

	printf("RAND_MAX = %d\n", RAND_MAX);

	for (i = 1; i < argc; ++i) {
		r = rand() % 100;
		printf("i + r = %d\n", i + r);
	}
	
	printf("r = %d\n", r);
	
	return 0;
}

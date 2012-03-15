#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	int n, i;

	assert(argc == 2);
	n = atoi(argv[1]);
	assert(n > 0 && n <= 1000);

	for (i = 0; i * 3 <= n; ++i) {
		printf("i = %d\n", i);
	}

	return 0;
}

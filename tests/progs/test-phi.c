#include <stdio.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
	int i, j;

	i = 0;
	do {
		++i;
	} while (i < argc);

	for (j = 0; j < i; ++i)
		printf("%lu\n", pthread_self());

	return 0;
}

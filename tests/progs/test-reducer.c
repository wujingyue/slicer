#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Error\n");
		exit(1);
	}
	printf("last arg = %s\n", argv[argc - 1]);
	return 0;
}

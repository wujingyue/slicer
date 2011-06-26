#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	int lb = atoi(argv[1]);
	int ub = atoi(argv[2]);
	int i;
	for (i = lb; i != ub; ++i)
		printf("output %d\n", i);
	return 0;
}


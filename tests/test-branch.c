#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 3)
		exit(1);
	int i = 0;
	if (i == 2)
		printf("i = %d\n", i);
	++i;
	if (i == 2)
		printf("i = %d\n", i);
	++i;
	if (i == 2)
		printf("i = %d\n", i);
	return 0;
}

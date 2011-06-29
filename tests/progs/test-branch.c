#include <stdio.h>
#include <stdlib.h>

volatile int i = 0;

int main(int argc, char *argv[]) {
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

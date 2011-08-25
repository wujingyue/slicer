#include <stdio.h>
#include <stdlib.h>

int main() {
	int i = 0;

	while (i < 100) {
		printf("i = %d\n", i);
		i += rand() % 5;
	}

	return 0;
}

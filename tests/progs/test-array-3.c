#include <stdio.h>
#include <stdlib.h>

#define MAX_P (1024)
#define SIZE (4096)

struct S {
	int *a;
	int *b;
} gp;

int main() {
	gp.a = malloc(SIZE);
	gp.b = malloc(SIZE);
	free(gp.a);
	free(gp.b);
	return 0;
}

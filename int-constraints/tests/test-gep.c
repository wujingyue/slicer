#include <stdio.h>

#define N (1024)

int a[N];

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; ++i)
		a[i] = i * i;
	int sum = 0;
	for (i = 0; i < argc; ++i)
		sum += a[i] * a[i];
	printf("%d\n", sum);
	return 0;
}


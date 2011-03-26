#include <stdio.h>

#define N (1024)

int a[N];

void foo() {
	int i;
	for (i = 0; i < N; ++i)
		a[i] = i;
}

void bar() {
	int sum = 0;
	int i;
	for (i = 0; i < N; ++i)
		sum += a[i];
	printf("sum = %d\n", sum);
}

int main() {
	foo();
	bar();
	return 0;
}


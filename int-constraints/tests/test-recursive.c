#include <stdio.h>

int helper(int n) {
	return fib(n - 1);
}

int fib(int n) {
	if (n == 0)
		return 0;
	if (n == 1)
		return 1;
	return helper(n) + helper(n - 1);
}

int main(int argc, char *argv[]) {
	assert(argc >= 2);
	int n = atoi(argv[1]);
	assert(n >= 0);
	printf("%d\n", fib(n));
	return 0;
}


#include <iostream>
using namespace std;

int fib(int n) {
	fprintf(stderr, "fib(%d)\n", n);
	if (n == 0)
		return 0;
	if (n == 1)
		return 1;
	return fib(n - 2) + fib(n - 1);
}

int main(int argc, char *argv[]) {
	cout << fib(atoi(argv[1])) << endl;
	return 0;
}

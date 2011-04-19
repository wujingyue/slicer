#include <stdio.h>

int a[5];

void foo() {
	fprintf(stderr, "%d\n", a[1]);
}

void bar() {
	a[1] = 3;
}

int main() {
	foo();
	bar();
	return 0;
}


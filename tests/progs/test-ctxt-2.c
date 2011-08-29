/**
 * Test whether the solver is able to realize functions that are executed
 * more than once. 
 */

#include <stdio.h>

void foo() __attribute__((noinline));

void foo(int a) {
	int b = a + 1;
	printf("b = %d\n", b);
}

int main() {
	foo(1);
	foo(2);
	return 0;
}

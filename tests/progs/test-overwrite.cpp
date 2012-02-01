#include <iostream>
#include <cstdio>
using namespace std;

volatile int a;

void foo() {
	printf("%d\n", a);
}

int main(int argc, char *argv[]) {
	if (argc == 0)
		a = 1;
	else
		a = 2;
	printf("%d\n", a);
	foo();
	return 0;
}

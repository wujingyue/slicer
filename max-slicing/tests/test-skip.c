#include <stdio.h>

void foo(int argc) {
	if (argc == 1) {
		printf("one argument\n");
	} else {
		printf("more arguments\n");
	}
}

int main(int argc, char *argv[]) {
	foo(argc);
	return 0;
}


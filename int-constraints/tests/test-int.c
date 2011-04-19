#include <stdio.h>

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; ++i)
		*argv[i] = '\0';
	return 0;
}


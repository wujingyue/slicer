#include <stdio.h>

int main(int argc, char *argv[]) {
	if (argc < 6)
		printf("%s\n", argv[argc - 1]);
	return 0;
}

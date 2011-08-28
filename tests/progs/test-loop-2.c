/**
 * Different iterations of a loop correspond to different contexts. 
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i + 1 < argc; ++i) {
		printf("this: %s\n", argv[i]);
		printf("next: %s\n", argv[i + 1]);
	}
	return 0;
}

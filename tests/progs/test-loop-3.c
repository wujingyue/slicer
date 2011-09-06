/**
 * Author: Jingyue
 *
 * The max-slicer should be able to slice this program according to 
 * the sync trace, even if there's no derived landmark. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
	int i;
	for (i = 1; i < argc; ++i) {
		int arg = atoi(argv[i]);
		if (arg)
			printf("%lu\n", pthread_self());
	}
	return 0;
}

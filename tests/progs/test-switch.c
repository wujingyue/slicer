#include <stdio.h>
#include <stdlib.h>

int postpass_partition_size = 10;
int CacheSize = 20;

int main(int argc, char *argv[]) {
	int c;

	while ((c = getchar()) != EOF) {
		switch (c) {
			case 'B': postpass_partition_size = atoi(argv[1]); break;
			case 'C': CacheSize = atoi(argv[2]); break;
			case 'h': fprintf(stderr, "help\n"); exit(0); break;
		}
	}

	printf("postpass_partition_size = %d\n", postpass_partition_size);
	printf("CacheSize = %d\n", CacheSize);

	return 0;
}

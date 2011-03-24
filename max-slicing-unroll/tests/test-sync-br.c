#include <stdio.h>
#include <string.h>

void sync() {
}

int main(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; i++) {
		sync();
		if (i % 2 == 0)
			sync();
	}
	return 0;
}


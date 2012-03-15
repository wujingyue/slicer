#include <iostream>
#include <cassert>
#include <cstdlib>
using namespace std;

int main(int argc, char *argv[]) {
	int sum = 0;
	for (int i = 1; i < argc; ++i)
		sum += atoi(argv[i]);
	assert(sum == argc - 1);

	return 0;
}

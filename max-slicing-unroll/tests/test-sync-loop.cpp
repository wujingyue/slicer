#include <iostream>
#include <cstring>
using namespace std;

int process(char *arg) {
	cerr << arg << endl;
}

int main(int argc, char *argv[]) {
	for (int i = 0; i < argc; ++i) {
		if (strlen(argv[i]) > 0)
			process(argv[i]);
	}
	return 0;
}


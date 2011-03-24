#include <iostream>
#include <cstring>
using namespace std;

int main(int argc, char *argv[]) {
	for (int i = 0; i < argc; ++i) {
		if (strlen(argv[i]) > 0)
			cerr << argv[i] << endl;
	}
	return 0;
}


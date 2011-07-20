#include <iostream>
#include <pthread.h>
using namespace std;

void my_malloc(int x) {
	cout << pthread_self() << endl;
	if (x) {
		cout << "non-zero\n";
		my_malloc(0);
	} else {
		cout << "zero\n";
	}
}

int main() {
	my_malloc(1);
	my_malloc(0);
	return 0;
}

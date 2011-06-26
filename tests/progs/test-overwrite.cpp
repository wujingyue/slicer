#include <iostream>
using namespace std;

volatile int a;

void foo(int v) {
	cout << v << endl;
}

int main() {
	a = 1;
	foo(a);
	a = 2;
	foo(a);
	return 0;
}

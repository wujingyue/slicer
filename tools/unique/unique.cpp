#include <iostream>
#include <vector>
#include <set>
using namespace std;

int main() {
	string line;
	set<string> all_decls;
	while (getline(cin, line)) {
		if (line.find("BITVECTOR(") != string::npos) {
			if (!all_decls.count(line)) {
				all_decls.insert(line);
				cout << line << endl;
			}
		} else {
			cout << line << endl;
		}
	}
	return 0;
}

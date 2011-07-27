#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>
#include <algorithm>
using namespace std;

struct Record {
	clock_t query_time;
	string str1, str2, str3;
};
bool operator<(const Record &a, const Record &b) {
	return a.query_time > b.query_time;
}

vector<Record> records;

bool read_line(string &line) {
	while (getline(cin, line)) {
		if (line != "")
			return true;
	}
	return false;
}

bool read_record(Record &r) {
	string line;
	if (!read_line(line))
		return false;
	istringstream iss(line);
	iss >> r.query_time;
	assert(read_line(r.str1));
	assert(read_line(r.str2));
	assert(read_line(r.str3));
	return true;
}

int main() {
	
	Record r;
	while (read_record(r))
		records.push_back(r);
	
	sort(records.begin(), records.end());
	for (size_t i = 0; i < records.size(); ++i) {
		cout << records[i].query_time << "\t" << records[i].str1 << endl;
		cout << records[i].str2 << endl << records[i].str3 << endl;
	}
	
	return 0;
}

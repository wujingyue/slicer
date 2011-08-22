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
	
	clock_t tot_provable = 0, tot_unprovable = 0;
	unsigned n_unprovables = 0, n_provables = 0;
	for (size_t i = 0; i < records.size(); ++i) {
		bool satisfiable = (records[i].str1.compare(0, 3, "may") == 0);
		bool correct = (records[i].str1[records[i].str1.length() - 1] == '1');
		if ((satisfiable && correct) || (!satisfiable && !correct)) {
			tot_unprovable += records[i].query_time;
			n_unprovables++;
		} else {
			tot_provable += records[i].query_time;
			n_provables++;
		}
	}
	cout << "Time spent on provable queries = " << tot_provable << "\n";
	cout << "# of provable queries = " << n_provables << "\n";
	cout << "Time spent on unprovable queries = " << tot_unprovable << "\n";
	cout << "# of unprovable queries = " << n_unprovables << "\n";
	
	return 0;
}

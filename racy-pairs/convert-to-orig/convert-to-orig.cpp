#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace std;

#include <boost/unordered_map.hpp>
using namespace boost;

unordered_map<unsigned, unsigned> new_ids_to_old;

void print_usage(int argc, char *argv[]) {
	fprintf(stderr, "%s <clone map>\n", argv[0]);
}

void read_clone_map(const char *clone_map_file) {
	ifstream fin(clone_map_file);
	string line;
	while (getline(fin, line)) {
		istringstream iss(line);
		int thr_id;
		size_t trunk_id;
		unsigned old_ins_id, new_ins_id;
		if (iss >> thr_id >> trunk_id >> old_ins_id >> new_ins_id) {
			new_ids_to_old[new_ins_id] = old_ins_id;
		}
	}
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		print_usage(argc, argv);
		exit(1);
	}
	
	read_clone_map(argv[1]);
	
	string line;
	vector<pair<unsigned, unsigned> > results;
	while (getline(cin, line)) {
		istringstream iss(line);
		unsigned a, b;
		if (iss >> a >> b) {
			assert(new_ids_to_old.count(a) && new_ids_to_old.count(b));
			unsigned id_a = new_ids_to_old[a];
			unsigned id_b = new_ids_to_old[b];
			if (id_a > id_b)
				swap(id_a, id_b);
			results.push_back(make_pair(id_a, id_b));
		}
	}

	sort(results.begin(), results.end());
	results.resize(unique(results.begin(), results.end()) - results.begin());
	for (size_t i = 0; i < results.size(); ++i)
		cout << results[i].first << ' ' << results[i].second << endl;

	return 0;
}


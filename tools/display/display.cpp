#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
using namespace std;

#include "slicer/trace.h"
#include "slicer/landmark-trace-record.h"
#include "slicer/trace-manager.h"
using namespace slicer;

void print_usage() {
	fprintf(stderr, "Usage: display-trace <full|landmark> < <trace file>\n");
}

void display_full_trace() {
	TraceRecord record;
	int idx = 0;
	while (cin.read((char *)&record, sizeof record)) {
		printf("%d: inst = %u, tid = %lu", idx, record.ins_id, record.raw_tid);
		if (record.raw_child_tid != INVALID_RAW_TID)
			printf(", child tid = %lu", record.raw_child_tid);
		printf("\n");
		++idx;
	}
}

void display_landmark_trace() {
	unsigned n_enforcing = 0, n_non_enforcing = 0;
	LandmarkTraceRecord record;
	DenseMap<unsigned, unsigned> ins_freq;
	while (cin.read((char *)&record, sizeof record)) {
		printf("%u: inst = %u, tid = %d",
				record.idx, record.ins_id, record.tid);
		if (record.child_tid != TraceManager::INVALID_TID)
			printf(", child tid = %d", record.child_tid);
		if (record.enforcing) {
			printf(" [enforcing]");
			++n_enforcing;
		} else {
			printf(" [non-enforcing]");
			++n_non_enforcing;
		}
		++ins_freq[record.ins_id];
		printf("\n");
	}

	printf("\n");
	printf("# of occurences of enforcing landmarks = %u\n", n_enforcing);
	printf("# of occurences of non-enforcing landmarks = %u\n",
			n_non_enforcing);

	printf("\n");
	vector<pair<unsigned, unsigned> > sorted_ins_freq;
	for (DenseMap<unsigned, unsigned>::iterator i = ins_freq.begin();
			i != ins_freq.end(); ++i) {
		// Note that we are reversing the order of the pair. 
		sorted_ins_freq.push_back(make_pair(i->second, i->first));
	}
	sort(sorted_ins_freq.begin(), sorted_ins_freq.end(),
			greater<pair<unsigned, unsigned> >());
	static const unsigned limit = 10;
	printf("Top %u executed landmarks:\n", limit);
	for (unsigned i = 0; i < limit; ++i) {
		if (i >= sorted_ins_freq.size())
			break;
		printf("Instruction %u: Frequency %u\n",
				sorted_ins_freq[i].second, sorted_ins_freq[i].first);
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_usage();
		exit(1);
	}
	if (strcmp(argv[1], "full") == 0) {
		display_full_trace();
	} else if (strcmp(argv[1], "landmark") == 0) {
		display_landmark_trace();
	} else {
		cout << "Error: Unrecognized mode: " << argv[1] << endl;
		return 1;
	}
	return 0;
}

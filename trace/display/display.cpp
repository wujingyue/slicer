#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
using namespace std;

#include "../trace.h"
#include "../landmark-trace-record.h"
#include "../trace-manager.h"
using namespace slicer;

void print_usage() {
	fprintf(stderr, "Usage: display <full|landmark> < <trace file>\n");
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_usage();
		exit(1);
	}
	if (strcmp(argv[1], "full") == 0) {
		TraceRecord record;
		int idx = 0;
		while (cin.read((char *)&record, sizeof record)) {
			printf("%d: inst = %u, tid = %lu", idx, record.ins_id, record.raw_tid);
			if (record.raw_child_tid != INVALID_RAW_TID)
				printf(", child tid = %lu", record.raw_child_tid);
			printf("\n");
			++idx;
		}
	} else if (strcmp(argv[1], "landmark") == 0) {
		LandmarkTraceRecord record;
		while (cin.read((char *)&record, sizeof record)) {
			printf("%u: inst = %u, tid = %d",
					record.idx, record.ins_id, record.tid);
			if (record.child_tid != TraceManager::INVALID_TID)
				printf(", child tid = %d", record.child_tid);
			printf(record.enforcing ? " [enforcing]" : " [non-enforcing]");
			printf("\n");
		}
	} else {
		cout << "Error: Unrecognized mode: " << argv[1] << endl;
		return 1;
	}
	return 0;
}

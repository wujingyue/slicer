#include <iostream>
#include <cstdlib>
#include <fstream>
using namespace std;

#include "../trace.h"
using namespace slicer;

void print_usage() {
	fprintf(stderr, "Usage: display <trace file>\n");
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_usage();
		exit(1);
	}
	ifstream fin(argv[1]);
	if (!fin) {
		fprintf(stderr, "Cannot open file %s\n", argv[1]);
		exit(1);
	}
	TraceRecord record;
	int idx = 0;
	while (fin.read((char *)&record, sizeof record)) {
		printf("%d: inst = %u, tid = %lu", idx, record.ins_id, record.raw_tid);
		if (record.raw_child_tid != INVALID_RAW_TID)
			printf(", child tid = %lu", record.raw_child_tid);
		printf("\n");
		++idx;
	}
	return 0;
}

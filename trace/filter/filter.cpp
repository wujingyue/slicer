#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
using namespace std;

#include "llvm/Support/CommandLine.h"
#include "common/util.h"
using namespace llvm;

#include "trace/landmark-trace-record.h"
using namespace slicer;

static cl::list<int> RegionStarts("s",
		cl::desc("IDs of the instructions that start a region"));
static cl::list<int> RegionEnds("e",
		cl::desc("IDs of the instructions that end a region"));
static cl::list<int> ThreadsToBeKept("t",
		cl::desc("IDs of the threads that should be kept"));
static cl::list<int> ThreadsToBeRemoved("nt",
		cl::desc("IDs of the threads that should be removed"));

void print_usage() {
	fprintf(stderr, "Usage: filter-landmark-trace [options: -s, -e, -t, -nt]"
		 " < <old landmark trace> > <new landmark trace>\n");
}

bool should_remove_thread(int tid) {
	if (ThreadsToBeKept.begin() != ThreadsToBeKept.end()) {
		// If found, returns false.
		// If not found, returns true.
		return find(ThreadsToBeKept.begin(), ThreadsToBeKept.end(), tid) ==
			ThreadsToBeKept.end();
	} else if (ThreadsToBeRemoved.begin() != ThreadsToBeRemoved.end()) {
		// If found, returns true.
		// If not found, returns false.
		return find(ThreadsToBeRemoved.begin(), ThreadsToBeRemoved.end(), tid) !=
			ThreadsToBeRemoved.end();
	}
	return false;
}

bool is_region_start(int ins_id) {
	return find(RegionStarts.begin(), RegionStarts.end(), ins_id) !=
		RegionStarts.end();
}

bool is_region_end(int ins_id) {
	return find(RegionEnds.begin(), RegionEnds.end(), ins_id) !=
		RegionEnds.end();
}

int main(int argc, char *argv[]) {
	if (1 < argc && (strcmp(argv[1], "-h") == 0 ||
				strcmp(argv[1], "--help") == 0)) {
		print_usage();
		exit(0);
	}

	cl::ParseCommandLineOptions(argc, argv,
			"Filter records in a landmark trace");

	if (ThreadsToBeKept.begin() != ThreadsToBeKept.end() &&
			ThreadsToBeRemoved.begin() != ThreadsToBeRemoved.end()) {
		cerr << "-t and -nt should not be specified at the same time.\n";
		exit(1);
	}
	if ((RegionStarts.begin() == RegionStarts.end()) ^
			(RegionEnds.begin() == RegionEnds.end())) {
		cerr << "Either specify both -s and -e or specify neither of them.\n";
		exit(1);
	}

	assert(ThreadsToBeRemoved.begin() != ThreadsToBeRemoved.end());
	assert(should_remove_thread(0));

	LandmarkTraceRecord record;
	map<int, bool> thr_in_region;
	bool in_region_initially = (RegionStarts.begin() == RegionStarts.end());
	while (cin.read((char *)&record, sizeof record)) {
		if (should_remove_thread(record.tid))
			continue;
		if (!thr_in_region.count(record.tid))
			thr_in_region[record.tid] = in_region_initially;
		if (is_region_start(record.ins_id))
			thr_in_region[record.tid] = true;
		if (is_region_end(record.ins_id))
			thr_in_region[record.tid] = false;
		if (thr_in_region[record.tid])
			cout.write((char *)&record, sizeof record);
	}

	return 0;
}

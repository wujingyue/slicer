#define DEBUG_TYPE "alias-query"

#include <sys/timeb.h>
#include <fstream>
#include <sstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "common/IDAssigner.h"
#include "common/util.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "slicer/iterate.h"
#include "slicer/query-driver.h"
#include "slicer/adv-alias.h"
#include "pointer-access.h"
using namespace slicer;

#define TIME_DIFFERENCE(time1, time0) (difftime((time1).time, (time0).time) + 0.001 * ((time1).millitm - (time0).millitm))

static RegisterPass<QueryDriver> X("drive-queries",
		"Issues alias queries to either bc2bdd or advanced-aa",
		false, true);

static cl::opt<string> QueryList("query-list",
		cl::desc("The input query list"));
static cl::opt<bool> UseAdvancedAA("use-adv-aa",
		cl::desc("Use the advanced AA if turned on"));
static cl::opt<int> SampleRate("sample",
		cl::desc("Sample a subset of queries: 1/sample of all queries will "
			"be picked"),
		cl::init(1));

char QueryDriver::ID = 0;

bool QueryDriver::runOnModule(Module &M) {
	read_queries();
	issue_queries();
	return false;
}

void QueryDriver::issue_queries() {
	timeb start_time;
	timeb end_time;

	errs() << "# of queries = " << queries.size() << "\n";

	for (size_t i = 0; i < queries.size(); ++i) {
		// Deterministic sampling to be fair.
		if (i % SampleRate != 0)
			continue;
		const Instruction *i1 = queries[i].first.ins, *i2 = queries[i].second.ins;
		if (!i1 || !i2) {
			results.push_back(AliasAnalysis::NoAlias);
		} else {
			vector<PointerAccess> accesses1 = get_pointer_accesses(i1);
			vector<PointerAccess> accesses2 = get_pointer_accesses(i2);
			if (UseAdvancedAA) {
				AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
				for (size_t j1 = 0; j1 < accesses1.size(); ++j1) {
					for (size_t j2 = 0; j2 < accesses2.size(); ++j2) {
						if (racy(accesses1[j1], accesses2[j2])) {
							ftime(&start_time);
							results.push_back(AAA.alias(
										queries[i].first.callstack, accesses1[j1].loc,
										queries[i].second.callstack, accesses2[j2].loc));
							ftime(&end_time);
							total_time += TIME_DIFFERENCE(end_time, start_time);
						}
					}
				}
			} else {
				BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
				vector<User *> ctxt1, ctxt2;
				for (size_t j = 0; j < queries[i].first.callstack.size(); ++j) {
					const Instruction *frame = queries[i].first.callstack[j];
					assert(is_call(frame));
					ctxt1.push_back(const_cast<Instruction *>(frame));
				}
				for (size_t j = 0; j < queries[i].second.callstack.size(); ++j) {
					const Instruction *frame = queries[i].second.callstack[j];
					assert(is_call(frame));
					ctxt2.push_back(const_cast<Instruction *>(frame));
				}
				for (size_t j1 = 0; j1 < accesses1.size(); ++j1) {
					for (size_t j2 = 0; j2 < accesses2.size(); ++j2) {
						if (racy(accesses1[j1], accesses2[j2])) {
							ftime(&start_time);
							results.push_back(BAA.alias(
										&ctxt1, accesses1[j1].loc, 0,
										&ctxt2, accesses2[j2].loc, 0));
							ftime(&end_time);
							total_time += TIME_DIFFERENCE(end_time, start_time);
						}
					}
				}
			}
		}
		raw_ostream::Colors color;
		if (results.back() == AliasAnalysis::NoAlias)
			color = raw_ostream::GREEN;
		else if (results.back() == AliasAnalysis::MustAlias)
			color = raw_ostream::BLUE;
		else
			color = raw_ostream::RED;
		errs().changeColor(color) << results.back(); errs().resetColor();
		DEBUG(dbgs() << "Query " << i << ": " << results.back() << "\n";);
	}
	errs() << "\n";
}

void QueryDriver::print(raw_ostream &O, const Module *M) const {
	unsigned n_no = 0, n_may = 0, n_must = 0;
	for (size_t i = 0; i < results.size(); ++i) {
		if (results[i] == AliasAnalysis::NoAlias)
			++n_no;
		else if (results[i] == AliasAnalysis::MayAlias)
			++n_may;
		else if (results[i] == AliasAnalysis::MustAlias)
			++n_must;
		else
			assert_not_supported();
	}
	O << "No: " << n_no << "; ";
	O << "May: " << n_may << "; ";
	O << "Must: " << n_must << ";\n";
	O << "Time: " << total_time / results.size() << " sec per query.\n";
}

void QueryDriver::read_queries() {
	assert(QueryList != "" && "Didn't specify the input query list");
	ifstream fin(QueryList.c_str());
	assert(fin && "Cannot open the input query list");

	queries.clear();
	string line;
	while (getline(fin, line)) {
		const regex e("([\\d\\s]+),([\\d\\s]+)");
		smatch what;
		if (regex_match(line, what, e)) {
			ContextedIns ci1, ci2;
			parse_contexted_ins(what[1].str(), ci1);
			parse_contexted_ins(what[2].str(), ci2);
			// ci1.ins or ci2.ins can be NULL if the translation fails.
			// In that case, the load is optimized.
			queries.push_back(make_pair(ci1, ci2));
		}
	}
}

void QueryDriver::parse_contexted_ins(const string &str, ContextedIns &ci) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	istringstream iss(str);
	unsigned ins_id;
	while (iss >> ins_id) {
		if (ins_id == (unsigned)-1)
			ci.callstack.push_back(NULL);
		else
			ci.callstack.push_back(IDA.getInstruction(ins_id));
	}
	assert(ci.callstack.size() > 0);
	ci.ins = ci.callstack.back();
	ci.callstack.pop_back();
}

void QueryDriver::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDAssigner>();
	if (UseAdvancedAA) {
		AU.addRequired<Iterate>();
		AU.addRequired<AdvancedAlias>();
	} else {
		AU.addRequired<BddAliasAnalysis>();
	}
	ModulePass::getAnalysisUsage(AU);
}

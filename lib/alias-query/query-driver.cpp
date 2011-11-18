/**
 * Author: Jingyue
 */

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
#include "common/InitializePasses.h"
#include "bc2bdd/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "slicer/iterate.h"
#include "slicer/query-driver.h"
#include "slicer/adv-alias.h"
#include "pointer-access.h"
#include "slicer/clone-info-manager.h"
using namespace slicer;

static cl::opt<string> QueryList("query-list",
		cl::desc("The input query list"));
static cl::opt<bool> UseAdvancedAA("use-adv-aa",
		cl::desc("Use the advanced AA if turned on"));
static cl::opt<bool> LoadLoad("driver-loadload",
		cl::desc("The query driver considers load-load aliases as well"));

INITIALIZE_PASS_BEGIN(QueryDriver, "drive-queries",
		"Issues alias queries to either bc2bdd or advanced-aa", false, true)
INITIALIZE_PASS_DEPENDENCY(IDAssigner)
INITIALIZE_PASS_DEPENDENCY(CloneInfoManager)
if (UseAdvancedAA) {
	INITIALIZE_PASS_DEPENDENCY(Iterate)
	INITIALIZE_PASS_DEPENDENCY(AdvancedAlias)
} else {
	INITIALIZE_PASS_DEPENDENCY(BddAliasAnalysis)
}
INITIALIZE_PASS_END(QueryDriver, "drive-queries",
		"Issues alias queries to either bc2bdd or advanced-aa", false, true)

char QueryDriver::ID = 0;

void QueryDriver::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDAssigner>();
	AU.addRequired<CloneInfoManager>();
	if (UseAdvancedAA) {
		AU.addRequired<Iterate>();
		AU.addRequired<AdvancedAlias>();
	} else {
		AU.addRequired<BddAliasAnalysis>();
	}
}

QueryDriver::QueryDriver(): ModulePass(ID), total_time(0.0) {
	initializeQueryTranslatorPass(*PassRegistry::getPassRegistry());
}

bool QueryDriver::runOnModule(Module &M) {
	read_queries();
	issue_queries();
	return false;
}

static inline double time_diff(timeb time1, timeb time0) {
	return difftime(time1.time, time0.time) +
		0.001 * (time1.millitm - time0.millitm);
}

void QueryDriver::issue_queries() {
	timeb start_time;
	timeb end_time;

	errs() << "# of queries = " << queries.size() << "\n";

	for (size_t i = 0; i < queries.size(); ++i) {
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
						if (LoadLoad || racy(accesses1[j1], accesses2[j2])) {
							ftime(&start_time);
							results.push_back(AAA.alias(
										queries[i].first.callstack, accesses1[j1].loc,
										queries[i].second.callstack, accesses2[j2].loc));
							ftime(&end_time);
							total_time += time_diff(end_time, start_time);
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
						if (LoadLoad || racy(accesses1[j1], accesses2[j2])) {
							ftime(&start_time);
							results.push_back(BAA.alias(
										&ctxt1, accesses1[j1].loc ,0,
										&ctxt2, accesses2[j2].loc, 0));
							ftime(&end_time);
							total_time += time_diff(end_time, start_time);
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
		errs().changeColor(color) << results.back();
		if (results.back() == AliasAnalysis::MayAlias) {
			CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
			errs() << "\nIns 1: " << *i1
			       << "\nAt function: " << i1->getParent()->getParent()->getName();
			if (CIM.has_clone_info(i1)) {
				CloneInfo ci = CIM.get_clone_info(i1);
				errs() << "\nClone info: "
				       << "thr_id = " << ci.thr_id
				       << " trunk_id = " << ci.trunk_id
				       << " orig_ins_id = " << ci.orig_ins_id;
			}
			Function::iterator it = const_cast<BasicBlock *>(i1->getParent());
			++it;
			BasicBlock::iterator next_ins = it->getFirstNonPHI();
			errs() << "\n\tNext instruction: " << *next_ins;
			if (CIM.has_clone_info(next_ins)) {
				CloneInfo ci = CIM.get_clone_info(next_ins);
				errs() << "\n\tClone info: "
				       << "thr_id = " << ci.thr_id
				       << " trunk_id = " << ci.trunk_id
				       << " orig_ins_id = " << ci.orig_ins_id;
			} else {
				++it;
				next_ins = it->getFirstNonPHI();
				errs() << "\n\t\tNext instruction: " << *next_ins;
				if (CIM.has_clone_info(next_ins)) {
					CloneInfo ci = CIM.get_clone_info(next_ins);
					errs() << "\n\t\tClone info: "
					       << "thr_id = " << ci.thr_id
					       << " trunk_id = " << ci.trunk_id
					       << " orig_ins_id = " << ci.orig_ins_id;
				}
			}
			errs() << "\nIns 2: " << *i2
			       << "\nAt function: " << i2->getParent()->getParent()->getName();
			if (CIM.has_clone_info(i2)) {
				CloneInfo ci = CIM.get_clone_info(i2);
				errs() << "\nClone info: "
				       << "thr_id = " << ci.thr_id
				       << " trunk_id = " << ci.trunk_id
				       << " orig_ins_id = " << ci.orig_ins_id;
			}
			it = const_cast<BasicBlock *>(i2->getParent());
			++it;
			next_ins = it->getFirstNonPHI();
			errs() << "\n\tNext instruction: " << *next_ins;
			if (CIM.has_clone_info(next_ins)) {
				CloneInfo ci = CIM.get_clone_info(next_ins);
				errs() << "\n\tClone info: "
				       << "thr_id = " << ci.thr_id
				       << " trunk_id = " << ci.trunk_id
				       << " orig_ins_id = " << ci.orig_ins_id;
			} else {
				++it;
				next_ins = it->getFirstNonPHI();
				errs() << "\n\t\tNext instruction: " << *next_ins;
				if (CIM.has_clone_info(next_ins)) {
					CloneInfo ci = CIM.get_clone_info(next_ins);
					errs() << "\n\t\tClone info: "
					       << "thr_id = " << ci.thr_id
					       << " trunk_id = " << ci.trunk_id
					       << " orig_ins_id = " << ci.orig_ins_id;
				}
			}
			errs() << "\n";
		}
		errs().resetColor();
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
	double n_total = n_no + n_may + n_must;
	O << "No: " << n_no << " (" << n_no / n_total << "); ";
	O << "May: " << n_may << " (" << n_may / n_total << "); ";
	O << "Must: " << n_must << " (" << n_must / n_total << ");\n";
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

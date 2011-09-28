#include <fstream>
#include <sstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "llvm/Support/CommandLine.h"
#include "common/IDAssigner.h"
#include "common/util.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "slicer/iterate.h"
#include "slicer/query-driver.h"
#include "slicer/adv-alias.h"
using namespace slicer;

static RegisterPass<QueryDriver> X("drive-queries",
		"Issues alias queries to either bc2bdd or advanced-aa",
		false, true);

static cl::opt<string> QueryList("query-list",
		cl::desc("The input query list"));
static cl::opt<bool> UseAdvancedAA("use-adv-aa",
		cl::desc("Use the advanced AA if turned on"));

char QueryDriver::ID = 0;

bool QueryDriver::runOnModule(Module &M) {
	read_queries();
	issue_queries();
	return false;
}

void QueryDriver::issue_queries() {
	for (size_t i = 0; i < queries.size(); ++i) {
		const Instruction *i1 = queries[i].first.ins, *i2 = queries[i].second.ins;
		const Value *v1 = (isa<StoreInst>(i1) ?
				cast<StoreInst>(i1)->getPointerOperand() :
				cast<LoadInst>(i1)->getPointerOperand());
		const Value *v2 = (isa<StoreInst>(i2) ?
				cast<StoreInst>(i2)->getPointerOperand() :
				cast<LoadInst>(i2)->getPointerOperand());
		if (UseAdvancedAA) {
			AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
			results.push_back(AAA.alias(
						queries[i].first.callstack, v1,
						queries[i].second.callstack, v2));
		} else {
			BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
			vector<User *> ctxt1, ctxt2;
			for (size_t j = 0; j < queries[i].first.callstack.size(); ++j)
				ctxt1.push_back(
						const_cast<Instruction *>(queries[i].first.callstack[j]));
			for (size_t j = 0; j < queries[i].second.callstack.size(); ++j)
				ctxt2.push_back(
						const_cast<Instruction *>(queries[i].second.callstack[j]));
			results.push_back(BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0));
		}
	}
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
	O << "Must: " << n_must << "; ";
	O << "May: " << n_may << "\n";
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
			assert(ci1.ins && ci2.ins);
			queries.push_back(make_pair(ci1, ci2));
		}
	}
}

void QueryDriver::parse_contexted_ins(const string &str, ContextedIns &ci) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	istringstream iss(str);
	unsigned ins_id;
	while (iss >> ins_id) {
		ci.callstack.push_back(IDA.getInstruction(ins_id));
		assert(ci.callstack.back());
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

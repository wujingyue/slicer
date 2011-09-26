#include <fstream>
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
		const Instruction *i1 = queries[i].first, *i2 = queries[i].second;
		const Value *v1 = (isa<StoreInst>(i1) ?
				cast<StoreInst>(i1)->getPointerOperand() :
				cast<LoadInst>(i1)->getPointerOperand());
		const Value *v2 = (isa<StoreInst>(i2) ?
				cast<StoreInst>(i2)->getPointerOperand() :
				cast<LoadInst>(i2)->getPointerOperand());
		if (UseAdvancedAA) {
			AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
			results.push_back(AAA.alias(v1, 0, v2, 0));
		} else {
			BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
			results.push_back(BAA.alias(v1, 0, v2, 0));
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

	IDAssigner &IDA = getAnalysis<IDAssigner>();
	
	queries.clear();
	string line;
	while (getline(fin, line)) {
		const regex e("(\\d+)\\s+(\\d+)");
		smatch what;
		if (regex_match(line, what, e)) {
			unsigned ins_id_1 = atoi(what[1].str().c_str());
			unsigned ins_id_2 = atoi(what[2].str().c_str());
			const Instruction *ins_1 = IDA.getInstruction(ins_id_1);
			const Instruction *ins_2 = IDA.getInstruction(ins_id_2);
			assert(ins_1 && ins_2);
			queries.push_back(make_pair(ins_1, ins_2));
		}
	}
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

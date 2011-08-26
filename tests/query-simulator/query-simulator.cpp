/**
 * Author: Jingyue
 */

#include <fstream>
#include <sstream>
#include <set>
#include <string>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "common/id-manager/IDAssigner.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "int/adv-alias.h"
#include "int/solve.h"
#include "int/iterate.h"

namespace slicer {
	struct QuerySimulator: public ModulePass {
		static char ID;

		QuerySimulator(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	
	private:
		void read_queries();
		void issue_queries();
		void fake_queries();
		void analyze_queries();
		void analyze(const Value *v);
		Clause *construct_inst_query(unsigned iid1, unsigned iid2);

		vector<QueryInfo> queries;
	};
}
using namespace slicer;

static RegisterPass<QuerySimulator> X(
		"simulate-queries", "Simulate the queries in a file");

static cl::opt<string> QueryFile(
		"queries",
		cl::desc("The file which contains the queries"),
		cl::init(""));

char QuerySimulator::ID = 0;

void QuerySimulator::read_queries() {
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	assert(QueryFile != "" && "Didn't specify the file containing the queries");
	ifstream fin(QueryFile.c_str());
	assert(fin && "Cannot open the specified file");
	string line;
	while (getline(fin, line)) {
		// <line> contains the query time.
		getline(fin, line);
		// <line> contains may/must. 
		bool satisfiable = (line.compare(0, 3, "may") == 0);
		getline(fin, line);
		// <line> contains v1.
		istringstream iss(line);
		char ch;
		unsigned vid_1;
		iss >> ch; assert(ch == '[');
		iss >> vid_1;
		iss >> ch; assert(ch == ']');
		getline(fin, line);
		// <line> contains v2.
		iss.str(line);
		unsigned vid_2;
		iss >> ch; assert(ch == '[');
		iss >> vid_2;
		iss >> ch; assert(ch == ']');
		const Value *v1 = IDA.getValue(vid_1); assert(v1);
		const Value *v2 = IDA.getValue(vid_2); assert(v2);
		// Field <result> is not used here. We redo the query anyway. 
		queries.push_back(QueryInfo(satisfiable, v1, v2, false));
	}

	dbgs() << "Read " << queries.size() << " queries.\n";
}

bool QuerySimulator::runOnModule(Module &M) {
#if 0
	read_queries();
	analyze_queries();
	issue_queries();
#endif
	fake_queries();
	return false;
}

Clause *QuerySimulator::construct_inst_query(unsigned iid1, unsigned iid2) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	Instruction *i1 = IDA.getInstruction(iid1); assert(i1);
	Instruction *i2 = IDA.getInstruction(iid2); assert(i2);
	assert(isa<StoreInst>(i1) || isa<LoadInst>(i1));
	assert(isa<StoreInst>(i2) || isa<LoadInst>(i2));
	const Value *v1 = i1->getOperand(isa<StoreInst>(i1) ? 1 : 0);
	const Value *v2 = i2->getOperand(isa<StoreInst>(i2) ? 1 : 0);
	return new Clause(new BoolExpr(CmpInst::ICMP_EQ,
				new Expr(v1), new Expr(v2)));
}

void QuerySimulator::fake_queries() {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	ifstream fin("for-jingyue/slicing.log");
	Clause *c = NULL;
	for (int i = 0; i < 32; ++i) {
		unsigned iid1, iid2;
		fin >> iid1 >> iid2;
		if (!c)
			c = construct_inst_query(iid1, iid2);
		else
			c = new Clause(Instruction::Or, c, construct_inst_query(iid1, iid2));
	}

	clock_t start = clock();
	errs() << SC.satisfiable(c) << "\n";
	errs() << "Time = " << clock() - start << "\n";

	assert(SC.provable(c) == false);

	delete c;
}

void QuerySimulator::analyze(const Value *v) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	bdd pointees = BAA.getPointeeSet(NULL, v, 0);
#if 0
	errs() << "size = " << BAA.getPointeeSetSize(pointees) << "\n";
#endif
	assert(BAA.getPointeeSetSize(pointees) == 1);
	BddAliasAnalysis::EnumeratedPointeeSet epts;
	BAA.enumeratePointeeSet(pointees, epts);
	for (set<const Value *>::const_iterator itr = epts.stackLocs.begin(),
			E = epts.stackLocs.end(); itr != E; ++itr)
		errs() << *(*itr) << "\n";
	for (set<const Value *>::const_iterator itr = epts.heapLocs.begin(),
			E = epts.heapLocs.end(); itr != E; ++itr)
		errs() << *(*itr) << "\n";
	for (set<const Value *>::const_iterator itr = epts.globalLocs.begin(),
			E = epts.globalLocs.end(); itr != E; ++itr)
		errs() << *(*itr) << "\n";
}

void QuerySimulator::analyze_queries() {
	dbgs() << "Analyzing all queries...\n";

	for (size_t i = 0; i < queries.size(); ++i) {
		errs() << "Query " << i << ":\n";
		const QueryInfo &query = queries[i];
		analyze(query.v1);
		analyze(query.v2);
	}
}

void QuerySimulator::issue_queries() {
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	// SolveConstraints &SC = getAnalysis<SolveConstraints>();

	dbgs() << "Issuing all queries...\n";
	
	for (size_t i = 0; i < queries.size(); ++i) {
		const QueryInfo &query = queries[i];
		if (query.satisfiable)
			AA.may_alias(query.v1, query.v2);
		else
			AA.must_alias(query.v1, query.v2);
	}
}

void QuerySimulator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDAssigner>();
	AU.addRequired<Iterate>();
	AU.addRequired<AdvancedAlias>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<BddAliasAnalysis>();
	ModulePass::getAnalysisUsage(AU);
}

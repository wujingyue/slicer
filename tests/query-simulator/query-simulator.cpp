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
#include "int/capture.h"
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
		Clause *construct_inst_query(unsigned iid1, unsigned iid2, int query_id);

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
#if 1
	fake_queries();
#endif
	return false;
}

Clause *QuerySimulator::construct_inst_query(unsigned iid1, unsigned iid2,
		int query_id) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	Instruction *i1 = IDA.getInstruction(iid1); assert(i1);
	Instruction *i2 = IDA.getInstruction(iid2); assert(i2);
	assert(isa<StoreInst>(i1) || isa<LoadInst>(i1));
	assert(isa<StoreInst>(i2) || isa<LoadInst>(i2));
	
	const Value *v1 = i1->getOperand(isa<StoreInst>(i1) ? 1 : 0);
	const Value *v2 = i2->getOperand(isa<StoreInst>(i2) ? 1 : 0);
	
	Expr *e1 = new Expr(v1), *e2 = new Expr(v2);
	
	CC.attach_context(e1, query_id * 2 + 1);
	CC.attach_context(e2, query_id * 2 + 2);
	
	return new Clause(new BoolExpr(CmpInst::ICMP_EQ, e1, e2));
}

void QuerySimulator::fake_queries() {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();

	ifstream fin("for-jingyue/slicing.log");
	vector<pair<unsigned, unsigned> > inst_queries;
	for (int i = 0; i < 2232; ++i) {
		unsigned iid1, iid2;
		fin >> iid1 >> iid2;
		inst_queries.push_back(make_pair(iid1, iid2));
	}

#if 0
	set<int> does_not_matter;

	for (int j = 99; j >= 0; --j) {
		errs() << "j = " << j << "\n";
		does_not_matter.insert(j);
		Clause *c = NULL;
		for (int i = 0; i < 100; ++i) {
			if (does_not_matter.count(i))
				continue;
			unsigned iid1 = inst_queries[i].first, iid2 = inst_queries[i].second;
			if (!c)
				c = construct_inst_query(iid1, iid2, i);
			else
				c = new Clause(Instruction::Or, c, construct_inst_query(iid1, iid2, i));
		}
		if (SC.provable(c) == false)
			does_not_matter.erase(j);
		delete c;
	}
	for (int i = 0; i < 100; ++i) {
		if (!does_not_matter.count(i))
			errs() << "i = " << i << "\n";
	}
#endif

#if 0
	Clause *c = NULL;
	for (int i = 0; i < 100; ++i) {
		unsigned iid1 = inst_queries[i].first, iid2 = inst_queries[i].second;
		
		Instruction *i1 = IDA.getInstruction(iid1); assert(i1);
		Instruction *i2 = IDA.getInstruction(iid2); assert(i2);
		assert(isa<StoreInst>(i1) || isa<LoadInst>(i1));
		assert(isa<StoreInst>(i2) || isa<LoadInst>(i2));
		const Value *v1 = i1->getOperand(isa<StoreInst>(i1) ? 1 : 0);
		const Value *v2 = i2->getOperand(isa<StoreInst>(i2) ? 1 : 0);

		if (BAA.alias(v1, 0, v2, 0) == AliasAnalysis::NoAlias)
			continue;

		if (!c)
			c = construct_inst_query(iid1, iid2, i);
		else
			c = new Clause(Instruction::Or, c, construct_inst_query(iid1, iid2, i));
	}

	clock_t start = clock();
	errs() << SC.satisfiable(c) << "\n";
	errs() << "Time = " << clock() - start << "\n";

	assert(SC.provable(c) == false);

	delete c;
#endif

#if 1
	for (int i = 0; i < 2322; ++i) {
		unsigned iid1 = inst_queries[i].first, iid2 = inst_queries[i].second;
		
		Instruction *i1 = IDA.getInstruction(iid1); assert(i1);
		Instruction *i2 = IDA.getInstruction(iid2); assert(i2);
		assert(isa<StoreInst>(i1) || isa<LoadInst>(i1));
		assert(isa<StoreInst>(i2) || isa<LoadInst>(i2));
		const Value *v1 = i1->getOperand(isa<StoreInst>(i1) ? 1 : 0);
		const Value *v2 = i2->getOperand(isa<StoreInst>(i2) ? 1 : 0);

		if (BAA.alias(v1, 0, v2, 0) == AliasAnalysis::NoAlias)
			continue;
		
		errs() << "Query " << i << ": ";
		Clause *c = construct_inst_query(iid1, iid2, i);
		errs() << SC.satisfiable(c) << "\n";
		delete c;
	}
#endif
}

void QuerySimulator::analyze(const Value *v) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	bdd pointees = BAA.getPointeeSet(NULL, v, 0);
	errs() << "size = " << BAA.getPointeeSetSize(pointees) << "\n";
	// assert(BAA.getPointeeSetSize(pointees) == 1);
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
		errs() << "Query " << i << ": ";
		const QueryInfo &query = queries[i];
		if (query.satisfiable)
			errs() << AA.may_alias(query.v1, query.v2) << "\n";
		else
			errs() << AA.must_alias(query.v1, query.v2) << "\n";
	}
}

void QuerySimulator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDAssigner>();
	AU.addRequired<Iterate>();
	AU.addRequired<AdvancedAlias>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<BddAliasAnalysis>();
	ModulePass::getAnalysisUsage(AU);
}

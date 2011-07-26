/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
using namespace slicer;

#include <sstream>
#include <iomanip>
#include <fstream>
using namespace std;

static RegisterPass<AdvancedAlias> X(
		"adv-alias",
		"Iterative alias analysis",
		false,
		true); // is analysis

char AdvancedAlias::ID = 0;

bool AdvancedAlias::runOnModule(Module &M) {
	recalculate(M);
	return false;
}

void AdvancedAlias::releaseMemory() {
}

void AdvancedAlias::print_average_query_time(raw_ostream &O) const {
	
	if (query_times.size() == 0)
		return;
	
	clock_t tot_time = 0;
	for (size_t i = 0; i < query_times.size(); ++i)
		tot_time += query_times[i].first;
	// raw_ostream does not print real numbers well. 
	ostringstream oss;
	oss << fixed << setprecision(5) <<
		(double)tot_time / CLOCKS_PER_SEC / query_times.size();
	O << "Average time on each query = " << oss.str() << "\n";

	string error_info;
	raw_fd_ostream fout("/tmp/query_times.txt", error_info);
	for (size_t i = 0; i < query_times.size(); ++i) {
		fout << query_times[i].first << "\n";
		if (query_times[i].second.satisfiable)
			fout << "may equal?\n";
		else
			fout << "must equal?\n";
		fout << "V1 = " << *query_times[i].second.v1 << "\n";
		fout << "V2 = " << *query_times[i].second.v2 << "\n";
	}
}

void AdvancedAlias::recalculate(Module &M) {
	// Clear the cache. 
	cache.clear();
	query_times.clear();
}


void AdvancedAlias::print(raw_ostream &O, const Module *M) const {
	O << "AAA cache size = " << get_cache_size() << "\n";
	print_average_query_time(O);
}

void AdvancedAlias::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<AliasAnalysis>();
	AU.addRequiredTransitive<BddAliasAnalysis>();
	AU.addRequiredTransitive<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

AliasAnalysis::AliasResult AdvancedAlias::alias(
		const Value *V1, unsigned V1Size,
		const Value *V2, unsigned V2Size) {

	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	if (BAA.alias(V1, V1Size, V2, V2Size) == NoAlias)
		return NoAlias;

	if (V1 > V2)
		swap(V1, V2);
	ConstValuePair p(V1, V2);
	if (cache.count(p))
		return cache.lookup(p);

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	AliasResult res;
	// TODO: <provable> takes much more time than satisfiable. Sometimes, we
	// only care about may-aliasing, so we could have a separate interface
	// doing must-aliasing. 
	
	clock_t start = clock();
	bool sat = SC.satisfiable(CmpInst::ICMP_EQ, V1, V2);
	query_times.push_back(make_pair(clock() - start, QueryInfo(true, V1, V2)));
	
	if (!sat) {
		res = NoAlias;
	} else {
		start = clock();
		bool pro = SC.provable(CmpInst::ICMP_EQ, V1, V2);
		query_times.push_back(make_pair(clock() - start, QueryInfo(false, V1, V2)));
		res = (pro ? MustAlias : MayAlias);
	}

	cache[p] = res;

	return res;
}

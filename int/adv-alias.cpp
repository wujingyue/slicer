#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/Support/CommandLine.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
using namespace slicer;

#include <sstream>
#include <iomanip>
using namespace std;

static RegisterPass<AdvancedAlias> X(
		"adv-alias",
		"Iterative alias analysis",
		false,
		true); // is analysis

char AdvancedAlias::ID = 0;

bool AdvancedAlias::runOnModule(Module &M) {
	return recalculate(M);
}

void AdvancedAlias::releaseMemory() {
	print_average_query_time();
}

void AdvancedAlias::print_average_query_time() {
	if (n_queries == 0)
		return;
	errs() << "Total time = " << tot_time << "\n";
	errs() << "# of queries = " << n_queries << "\n";
	ostringstream oss;
	oss << fixed << setprecision(5) <<
		(double)tot_time / CLOCKS_PER_SEC / n_queries;
	errs() << "Average time on each query = " << oss.str() << "\n";
}

bool AdvancedAlias::recalculate(Module &M) {
	// Clear the cache. 
	errs() << "AAA cache size = " << get_cache_size() << "\n";
	cache.clear();
	print_average_query_time();
	tot_time = 0;
	n_queries = 0;
	return false;
}

void AdvancedAlias::print(raw_ostream &O, const Module *M) const {
	// Not sure what to do yet.
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

#if 1
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	if (BAA.alias(V1, V1Size, V2, V2Size) == NoAlias)
		return NoAlias;
#else
	AliasAnalysis &BAA = getAnalysis<AliasAnalysis>();
	if (BAA.alias(V1, V1Size, V2, V2Size) == NoAlias)
		return NoAlias;
#endif

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
	if (!SC.satisfiable(CmpInst::ICMP_EQ, V1, V2))
		res = NoAlias;
	else if (SC.provable(CmpInst::ICMP_EQ, V1, V2))
		res = MustAlias;
	else
		res = MayAlias;
	cache[p] = res;

	tot_time += clock() - start;
	n_queries += (res == NoAlias ? 1 : 2);

	return res;
}

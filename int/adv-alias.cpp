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
	raw_fd_ostream fout("/tmp/query-times.txt", error_info);
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
	DenseMap<ConstValuePair, bool> old_may_cache(may_cache);
	DenseMap<ConstValuePair, bool> old_must_cache(must_cache);
	may_cache.clear();
	must_cache.clear();

	// OPT: Retain not-may and must results. 
	// Retain not-may results. 
	for (DenseMap<ConstValuePair, bool>::iterator it = old_may_cache.begin();
			it != old_may_cache.end(); ++it) {
		if (it->second == false)
			may_cache.insert(*it);
	}
	// Retain must results. 
	for (DenseMap<ConstValuePair, bool>::iterator it = old_must_cache.begin();
			it != old_must_cache.end(); ++it) {
		if (it->second == true)
			must_cache.insert(*it);
	}
	// Clear <query_times> which is for statistics. 
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

bool AdvancedAlias::must_alias(const Use *u1, const Use *u2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	const Value *v1 = u1->get(), *v2 = u2->get();
	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;

	if (v1 > v2)
		swap(v1, v2);
	ConstValuePair p(v1, v2);
	if (must_cache.count(p) && must_cache.lookup(p))
		return true;

	return SC.provable(CmpInst::ICMP_EQ, u1, u2);
}

bool AdvancedAlias::must_alias(const Value *v1, const Value *v2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;

	if (v1 > v2)
		swap(v1, v2);
	ConstValuePair p(v1, v2);

	bool pro;
	if (must_cache.count(p)) {
		pro = must_cache.lookup(p);
	} else {
		clock_t start = clock();
		pro = SC.provable(CmpInst::ICMP_EQ, v1, v2);
		query_times.push_back(make_pair(clock() - start, QueryInfo(false, v1, v2)));
		must_cache[p] = pro;
		if (pro) {
			// OPT: If we find they must equal, then they also may equal. 
			may_cache[p] = true;
		}
	}

	return pro;
}

bool AdvancedAlias::may_alias(const Use *u1, const Use *u2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	const Value *v1 = u1->get(), *v2 = u2->get();
	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;

	if (v1 > v2)
		swap(v1, v2);
	ConstValuePair p(v1, v2);
	if (may_cache.count(p) && may_cache.lookup(p) == false)
		return false;

	return SC.satisfiable(CmpInst::ICMP_EQ, u1, u2);
}

bool AdvancedAlias::may_alias(const Value *v1, const Value *v2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;

	if (v1 > v2)
		swap(v1, v2);
	ConstValuePair p(v1, v2);

	bool sat;
	if (may_cache.count(p)) {
		sat = may_cache.lookup(p);
	} else {
		clock_t start = clock();
		sat = SC.satisfiable(CmpInst::ICMP_EQ, v1, v2);
		query_times.push_back(make_pair(clock() - start, QueryInfo(true, v1, v2)));
		may_cache[p] = sat;
	}

	return sat;
}

AliasAnalysis::AliasResult AdvancedAlias::alias(
		const Value *v1, unsigned v1_size,
		const Value *v2, unsigned v2_size) {

	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, v1_size, v2, v2_size) == NoAlias)
		return NoAlias;

	if (v1 > v2)
		swap(v1, v2);
	ConstValuePair p(v1, v2);

	bool sat;
	if (may_cache.count(p)) {
		sat = may_cache.lookup(p);
	} else {
		clock_t start = clock();
		sat = SC.satisfiable(CmpInst::ICMP_EQ, v1, v2);
		query_times.push_back(make_pair(clock() - start, QueryInfo(true, v1, v2)));
		may_cache[p] = sat;
	}
	
	if (!sat)
		return NoAlias;

	bool pro;
	if (must_cache.count(p)) {
		pro = must_cache.lookup(p);
	} else {
		clock_t start = clock();
		pro = SC.provable(CmpInst::ICMP_EQ, v1, v2);
		query_times.push_back(make_pair(clock() - start, QueryInfo(false, v1, v2)));
		must_cache[p] = pro;
	}

	return (pro ? MustAlias : MayAlias);
}

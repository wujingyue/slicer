/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "common/IDAssigner.h"
using namespace llvm;

#include "slicer/capture.h"
#include "slicer/solve.h"
#include "slicer/adv-alias.h"
using namespace slicer;

#include <sstream>
#include <iomanip>
#include <fstream>
using namespace std;

static RegisterPass<AdvancedAlias> X("adv-alias",
		"Iterative alias analysis",
		false, true); // is analysis

STATISTIC(NumCacheHits, "Number of cache hits");
STATISTIC(NumCacheMisses, "Number of cache misses");

char AdvancedAlias::ID = 0;

bool AdvancedAlias::runOnModule(Module &M) {
	recalculate(M);
	return false;
}

void AdvancedAlias::releaseMemory() {
}

static bool by_first(const pair<clock_t, QueryInfo> &a,
		const pair<clock_t, QueryInfo>& b) {
	return a.first > b.first;
}

void AdvancedAlias::print_average_query_time(raw_ostream &O) const {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
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

	vector<pair<clock_t, QueryInfo> > sorted_query_times(query_times);
	sort(sorted_query_times.begin(), sorted_query_times.end(), by_first);

	string error_info;
	raw_fd_ostream fout("/tmp/query-times.txt", error_info);
	for (size_t i = 0; i < sorted_query_times.size(); ++i) {
		fout << sorted_query_times[i].first << "\n";
		if (sorted_query_times[i].second.satisfiable)
			fout << "may equal?";
		else
			fout << "must equal?";
		fout << " " << sorted_query_times[i].second.result << "\n";
		const Value *v1 = sorted_query_times[i].second.v1;
		const Value *v2 = sorted_query_times[i].second.v2;
		fout << "[" << IDA.getValueID(v1) << "] " << *v1 << "\n";
		fout << "[" << IDA.getValueID(v2) << "] " << *v2 << "\n";
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
		if (it->second == false) {
			may_cache.insert(*it);
			must_cache.insert(*it);
		}
	}
	// Retain must results. 
	for (DenseMap<ConstValuePair, bool>::iterator it = old_must_cache.begin();
			it != old_must_cache.end(); ++it) {
		if (it->second == true) {
			must_cache.insert(*it);
			may_cache.insert(*it);
		}
	}
	// Clear <query_times> which is for statistics. 
	query_times.clear();
}


void AdvancedAlias::print(raw_ostream &O, const Module *M) const {
	O << "AdvancedAA cache size = " << get_cache_size() << "\n";
	print_average_query_time(O);
}

void AdvancedAlias::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<IDAssigner>();
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

	bool pro;
	if (check_must_cache(v1, v2, pro)) {
		if (pro)
			return true;
	}
	
	return SC.provable(CmpInst::ICMP_EQ,
			ConstInstList(), u1, ConstInstList(), u2);
}

bool AdvancedAlias::must_alias(const Value *v1, const Value *v2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;
	if (v1 == v2)
		return true;

	bool pro;
	if (!check_must_cache(v1, v2, pro)) {
		clock_t start = clock();
		pro = SC.provable(CmpInst::ICMP_EQ,
				ConstInstList(), v1, ConstInstList(), v2);
		query_times.push_back(make_pair(
					clock() - start, QueryInfo(false, v1, v2, pro)));
		add_to_must_cache(v1, v2, pro);
	}
	return pro;
}

bool AdvancedAlias::may_alias(const Use *u1, const Use *u2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	const Value *v1 = u1->get(), *v2 = u2->get();
	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;

	bool sat;
	if (check_may_cache(v1, v2, sat)) {
		if (!sat)
			return false;
	}

	return SC.satisfiable(CmpInst::ICMP_EQ,
			ConstInstList(), u1, ConstInstList(), u2);
}

bool AdvancedAlias::may_alias(const Value *v1, const Value *v2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, 0, v2, 0) == NoAlias)
		return false;
	if (v1 == v2)
		return true;

	bool sat;
	if (!check_may_cache(v1, v2, sat)) {
		clock_t start = clock();
		sat = SC.satisfiable(CmpInst::ICMP_EQ,
				ConstInstList(), v1, ConstInstList(), v2);
		query_times.push_back(make_pair(
					clock() - start, QueryInfo(true, v1, v2, sat)));
		add_to_may_cache(v1, v2, sat);
	}
	return sat;
}

AliasAnalysis::AliasResult AdvancedAlias::alias(
		const ConstInstList &c1, const Value *v1,
		const ConstInstList &c2, const Value *v2) {
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	// Try context-sensitive BddAliasAnalysis first. 
	if (BAA.isAnalysisContextSensitive()) {
		vector<User *> ctxt1, ctxt2;
		for (size_t i = 0; i < c1.size(); ++i)
			ctxt1.push_back(const_cast<Instruction *>(c1[i]));
		for (size_t i = 0; i < c2.size(); ++i)
			ctxt2.push_back(const_cast<Instruction *>(c2[i]));
		if (BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0) == NoAlias)
			return NoAlias;
	}

	// Context-insensitive version is much faster. 
	if (AliasAnalysis::alias(v1, 0, v2, 0) == NoAlias)
		return NoAlias;

	// TODO: Caching
	if (!SC.satisfiable(CmpInst::ICMP_EQ, c1, v1, c2, v2))
		return NoAlias;
	if (SC.provable(CmpInst::ICMP_EQ, c1, v1, c2, v2))
		return MustAlias;
	return MayAlias;
}

AliasAnalysis::AliasResult AdvancedAlias::alias(
		const Location &L1, const Location &L2) {
	const Value *v1 = L1.Ptr, *v2 = L2.Ptr;
	uint64_t v1_size = L1.Size, v2_size = L2.Size;

	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	if (BAA.alias(v1, v1_size, v2, v2_size) == NoAlias)
		return NoAlias;

	bool sat;
	if (!check_may_cache(v1, v2, sat)) {
		clock_t start = clock();
		sat = SC.satisfiable(CmpInst::ICMP_EQ,
				ConstInstList(), v1, ConstInstList(), v2);
		query_times.push_back(
				make_pair(clock() - start, QueryInfo(true, v1, v2, sat)));
		add_to_may_cache(v1, v2, sat);
	}
	if (!sat)
		return NoAlias;

	bool pro;
	if (!check_must_cache(v1, v2, pro)) {
		clock_t start = clock();
		pro = SC.provable(CmpInst::ICMP_EQ,
				ConstInstList(), v1, ConstInstList(), v2);
		query_times.push_back(make_pair(
					clock() - start, QueryInfo(false, v1, v2, pro)));
		add_to_must_cache(v1, v2, pro);
	}
	return (pro ? MustAlias : MayAlias);
}

bool AdvancedAlias::check_may_cache(
		const Value *v1, const Value *v2, bool &res) {
	ConstValuePair p = make_ordered_value_pair(v1, v2);
	DenseMap<ConstValuePair, bool>::iterator it = may_cache.find(p);
	if (it != may_cache.end()) {
		++NumCacheHits;
		res = it->second;
		return true;
	} else {
		++NumCacheMisses;
		return false;
	}
}

bool AdvancedAlias::check_must_cache(
		const Value *v1, const Value *v2, bool &res) {
	ConstValuePair p = make_ordered_value_pair(v1, v2);
	DenseMap<ConstValuePair, bool>::iterator it = must_cache.find(p);
	if (it != must_cache.end()) {
		++NumCacheHits;
		res = it->second;
		return true;
	} else {
		++NumCacheMisses;
		return false;
	}
}

void AdvancedAlias::add_to_may_cache(
		const Value *v1, const Value *v2, bool res) {
	ConstValuePair p = make_ordered_value_pair(v1, v2);
	may_cache[p] = res;
	if (!res)
		must_cache[p] = res;
}

void AdvancedAlias::add_to_must_cache(
		const Value *v1, const Value *v2, bool res) {
	ConstValuePair p = make_ordered_value_pair(v1, v2);
	must_cache[p] = res;
	if (res)
		may_cache[p] = res;
}

ConstValuePair AdvancedAlias::make_ordered_value_pair(
		const Value *v1, const Value *v2) {
	if (v1 > v2)
		swap(v1, v2);
	return make_pair(v1, v2);
}

void AdvancedAlias::get_must_alias_pairs(vector<ConstValuePair> &result) const {
	result.clear();
	for (DenseMap<ConstValuePair, bool>::const_iterator it = must_cache.begin();
			it != must_cache.end(); ++it) {
		if (it->second == true)
			result.push_back(it->first);
	}
}

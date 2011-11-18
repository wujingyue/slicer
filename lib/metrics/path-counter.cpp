/**
 * Author: Jingyue
 */

#include <sstream>
using namespace std;

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "common/callgraph-fp.h"
#include "common/util.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "path-counter.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(PathCounter, "count-paths",
		"Count the paths", false, true)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_END(PathCounter, "count-paths",
		"Count the paths", false, true)

static cl::opt<int> NumIterations("iter",
		cl::desc("Number of iterations"),
		cl::init(1));

char PathCounter::ID = 0;

void PathCounter::dfs(BasicBlock *x, unsigned &cur_time) {
	if (start_time.count(x))
		return;

	start_time[x] = ++cur_time;
	vector<Edge> neighbors = g.lookup(x);
	for (size_t j = 0; j < neighbors.size(); ++j)
		dfs(neighbors[j].target, cur_time);
	finish_time[x] = ++cur_time;
}

bool PathCounter::runOnModule(Module &M) {
	CallGraphFP &CG = getAnalysis<CallGraphFP>();

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (succ_iterator si = succ_begin(bb); si != succ_end(bb); ++si)
				g[bb].push_back(Edge(*si, true));
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (is_call(ins)) {
					FuncList callees = CG.get_called_functions(ins);
					for (size_t i = 0; i < callees.size(); ++i) {
						Function *callee = callees[i];
						if (callee && !callee->isDeclaration())
							g[bb].push_back(Edge(callee->begin(), false));
					}
				}
			}
		}
	}

	Function *main = NULL;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (is_main(f)) {
			assert(!main);
			main = f;
		}
	}
	assert(main && !main->isDeclaration());

	unsigned cur_time = 0;
	dfs(main->begin(), cur_time);

	vector<pair<unsigned, BasicBlock *> > tmp;
	for (DenseMap<BasicBlock *, unsigned>::iterator itr = finish_time.begin();
			itr != finish_time.end(); ++itr)
		tmp.push_back(make_pair(itr->second, itr->first));
	sort(tmp.begin(), tmp.end()); // Reverse topological order
	vector<BasicBlock *> reverse_topo_order;
	for (size_t i = 0; i < tmp.size(); ++i)
		reverse_topo_order.push_back(tmp[i].second);

	for (int iter = 0; iter < NumIterations; ++iter) {
		for (size_t i = 0; i < reverse_topo_order.size(); ++i) {
			BasicBlock *x = reverse_topo_order[i];
			long double new_n_paths = compute_num_paths(x);
			n_paths[x] = new_n_paths;
		}
	}

	return false;
}

long double PathCounter::compute_num_paths(BasicBlock *x) {
	CallGraphFP &CG = getAnalysis<CallGraphFP>();

	long double intra_bb = 1;
	for (BasicBlock::iterator ins = x->begin(); ins != x->end(); ++ins) {
		if (is_call(ins)) {
			long double sum = 0;
			FuncList callees = CG.get_called_functions(ins);
			for (size_t i = 0; i < callees.size(); ++i) {
				Function *callee = callees[i];
				if (callee && !callee->isDeclaration())
					sum += n_paths[callee->begin()];
				else
					sum += 1;
			}
			if (abs(sum) <= 1e-8)
				sum = 1;
			intra_bb *= sum;
		}
	}

	long double inter_bb = 0;
	for (succ_iterator si = succ_begin(x); si != succ_end(x); ++si)
		inter_bb += n_paths[*si];
	if (abs(inter_bb) <= 1e-8)
		inter_bb = 1;

	long double result = intra_bb * inter_bb;
#if 0
	errs() << "compute_num_path " << x->getParent()->getName() << "."
		<< x->getName() << ": " << result << "\n";
#endif
	return result;
}

void PathCounter::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<CallGraphFP>();
}

void PathCounter::print(raw_ostream &O, const Module *M) const {
	long double res = 0;
	for (DenseMap<BasicBlock *, long double>::const_iterator
			itr = n_paths.begin(); itr != n_paths.end(); ++itr)
		res = max(res, itr->second);
	ostringstream oss;
	oss << res;
	oss.flush();
	O << "Maximum # of paths = " << oss.str() << "\n";
}

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/CFG.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/include/util.h"
using namespace llvm;

#include <vector>
using namespace std;

#include "exec-once.h"

namespace {

	static RegisterPass<slicer::ExecOnce> X(
			"exec-once",
			"Identify instructions that can be executed only once",
			false,
			true);
}

namespace slicer {

	void ExecOnce::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		// TODO:
		AU.addRequired<CallGraph>();
		AU.addRequired<CallGraphFP>();
		ModulePass::getAnalysisUsage(AU);
	}

	void ExecOnce::print(raw_ostream &O, const Module *M) const {
		// TODO
	}

	bool ExecOnce::runOnModule(Module &M) {
		identify_twice_bbs(M);
		identify_twice_funcs(M);
		return false;
	}

	void ExecOnce::identify_starting_funcs(Module &M, FuncSet &starts) {
		starts.clear();
		// Identify recursive functions. 
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		CallGraph &raw_CG = CG;
		for (scc_iterator<CallGraph *> si = scc_begin(&raw_CG),
				E = scc_end(&raw_CG); si != E; ++si) {
			const vector<CallGraphNode *> &scc = *si;
			assert(!scc.empty());
			if (scc.size() > 1) {
				// All functions in this SCC are recursive. 
				for (size_t i = 0; i < scc.size(); ++i) {
					Function *f = scc[i]->getFunction();
					if (f)
						starts.insert(f);
				}
			} else {
				// If it calls itself, then it's recursive. 
				CallGraphNode *node = scc[0];
				for (unsigned i = 0; i < node->size(); ++i) {
					CallGraphNode *callee = (*node)[i];
					if (callee == node)
						starts.insert(node->getFunction());
				}
			}
		} // for scc
		// Identify functions that are called by multiple call sites. 
		forallfunc(M, fi) {
			if (CG.get_call_sites(fi).size() > 1)
				starts.insert(fi);
		}
		// Identify functions that are called inside a loop. 
		forall(BBSet, it, twice_bbs) {
			BasicBlock *bb = *it;
			forall(BasicBlock, ii, *bb) {
				if (is_call(ii) && !is_intrinsic_call(ii)) {
					FuncList callees = CG.get_called_functions(ii);
					for (size_t j = 0; j < callees.size(); ++j)
						starts.insert(callees[j]);
				}
			}
		}
	}

	void ExecOnce::identify_twice_funcs(Module &M) {
		// Identify starting functions. 
		FuncSet starts;
		identify_starting_funcs(M, starts);
		// Propagate via the call graph. 
		twice_funcs.clear();
		forall(FuncSet, it, starts)
			propagate_via_cg(*it);
	}

	void ExecOnce::propagate_via_cg(Function *f) {
		if (twice_funcs.count(f))
			return;
		twice_funcs.insert(f);
		CallGraph &CG = getAnalysis<CallGraphFP>();
		// Operator [] does not change the function mapping. 
		CallGraphNode *x = CG[f];
		for (unsigned i = 0; i < x->size(); ++i) {
			CallGraphNode *y = (*x)[i];
			propagate_via_cg(y->getFunction());
		}
	}

	void ExecOnce::identify_twice_bbs(Module &M) {
		twice_bbs.clear();
		forallfunc(M, fi) {
			if (fi->isDeclaration())
				continue;
			// GraphTraits<Module::iterator> is not defined. 
			// Have to convert it to Function *. 
			Function *f = fi;
			for (scc_iterator<Function *> si = scc_begin(f), E = scc_end(f);
					si != E; ++si) {
				const vector<BasicBlock *> &scc = *si;
				// TODO: is there an interface to test if a node is inside a cycle? 
				assert(!scc.empty());
				if (scc.size() > 1) {
					for (size_t i = 0; i < scc.size(); ++i)
						twice_bbs.insert(scc[i]);
				} else {
					BasicBlock *bb = scc[0];
					for (succ_iterator it = succ_begin(bb); it != succ_end(bb); ++it) {
						if (*it == bb)
							twice_bbs.insert(bb);
					}
				}
			} // for scc
		}
	}

	bool ExecOnce::executed_once(Instruction *ins) const {
		return executed_once(ins->getParent());
	}

	bool ExecOnce::executed_once(BasicBlock *bb) const {
		return !twice_funcs.count(bb->getParent()) && !twice_bbs.count(bb);
	}

	bool ExecOnce::executed_once(Function *func) const {
		return !twice_funcs.count(func);
	}

	char ExecOnce::ID = 0;
}


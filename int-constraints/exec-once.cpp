#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/CFG.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/include/util.h"
using namespace llvm;

#include <vector>
using namespace std;

#include "exec-once.h"
#include "config.h"

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
		O << "List of BBs that can be executed only once:\n";
		forallconst(Module, fi, *M) {
			forallconst(Function, bi, *fi) {
				const BasicBlock *bb = bi;
				if (executed_once(const_cast<BasicBlock *>(bb))) {
					O << "\t" << bb->getParent()->getNameStr() << "."
						<< bb->getNameStr() << "\n";
				}
			}
		}
		O << "List of reachable functions:\n";
		forallconst(FuncSet, it, reachable_funcs)
			O << "\t" << (*it)->getNameStr() << "\n";
	}

	bool ExecOnce::runOnModule(Module &M) {
		identify_twice_bbs(M);
		identify_twice_funcs(M);
		identify_reachable_funcs(M);
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
		// Check <starts>.
		// No starting point can be NULL. 
		forall(FuncSet, it, starts)
			assert(*it);
	}

	void ExecOnce::identify_twice_funcs(Module &M) {
		// Identify starting functions. 
		FuncSet starts;
		identify_starting_funcs(M, starts);
		// Propagate via the call graph. 
		twice_funcs.clear();
		forall(FuncSet, it, starts)
			propagate_via_cg(*it, twice_funcs);
	}

	void ExecOnce::identify_reachable_funcs(Module &M) {
		Function *main = M.getFunction("main");
		assert(main && "Cannot find the main function");
		reachable_funcs.clear();
		propagate_via_cg(main, reachable_funcs);
	}

	void ExecOnce::propagate_via_cg(Function *f, FuncSet &visited) {
		// The call graph contains some external nodes which don't represent
		// any function. 
		if (!f)
			return;
		if (visited.count(f))
			return;
		visited.insert(f);
		CallGraph &CG = getAnalysis<CallGraphFP>();
		// Operator [] does not change the function mapping. 
		CallGraphNode *x = CG[f];
		for (unsigned i = 0; i < x->size(); ++i) {
			CallGraphNode *y = (*x)[i];
			propagate_via_cg(y->getFunction(), visited);
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

	bool ExecOnce::executed_once(const Instruction *ins) const {
		return executed_once(ins->getParent());
	}

	bool ExecOnce::executed_once(const BasicBlock *b) const {
		BasicBlock *bb = const_cast<BasicBlock *>(b);
#if 0
		forallconst(FuncSet, it, twice_funcs)
			cerr << "twice_funcs: " << (*it)->getNameStr() << endl;
		forallconst(BBSet, it, twice_bbs) {
			cerr << "twice_bbs: " << (*it)->getParent()->getNameStr() << "."
				<< (*it)->getNameStr() << endl;
		}
#endif
		return !twice_funcs.count(bb->getParent()) && !twice_bbs.count(bb);
	}

	bool ExecOnce::executed_once(const Function *f) const {
		Function *func = const_cast<Function *>(f);
		return !twice_funcs.count(func);
	}

	bool ExecOnce::not_executed(const Instruction *ins) const {
		return not_executed(ins->getParent());
	}

	bool ExecOnce::not_executed(const BasicBlock *bb) const {
		return not_executed(bb->getParent());
	}
	
	bool ExecOnce::not_executed(const Function *func) const {
		return !reachable_funcs.count(const_cast<Function *>(func));
	}

	char ExecOnce::ID = 0;
}


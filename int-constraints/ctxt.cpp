#include "llvm/ADT/SCCIterator.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include "ctxt.h"

// #define COLLAPSE_RECURSIVE

namespace {

	static RegisterPass<slicer::CountCtxts> X(
			"count-ctxts",
			"Count the number of calling contexts of each function",
			false,
			true);
}

namespace slicer {

	char CountCtxts::ID = 0;

	bool CountCtxts::runOnModule(Module &M) {
		
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		CallGraph &raw_CG = CG;
		// scc_iterator iterates all SCCs in a reverse topological order. 
		vector<scc_iterator<CallGraph *> > topo_order;
		for (scc_iterator<CallGraph *> si = scc_begin(&raw_CG);
				si != scc_end(&raw_CG); ++si) {
			topo_order.push_back(si);
		}
		// Therefore, we call <reverse> here to get the topological order. 
		reverse(topo_order.begin(), topo_order.end());
		assert(topo_order.size() > 0);
		// Initialize <n_ctxts> and build the table <node_to_scc>. 
		n_ctxts.clear();
		for (size_t i = 0; i < topo_order.size(); ++i) {
			n_ctxts.push_back(0);
			scc_iterator<CallGraph *> si = topo_order[i];
			forall(vector<CallGraphNode *>, it, *si)
				node_to_scc[*it] = i;
		}
		// Dynamic programming. 
		assert(node_to_scc.count(CG.getRoot()));
		n_ctxts[node_to_scc.lookup(CG.getRoot())] = 1;
		for (size_t i = 0; i < topo_order.size(); ++i) {
			if (n_ctxts[i] == 0)
				continue;
			scc_iterator<CallGraph *> si = topo_order[i];
#ifndef COLLAPSE_RECURSIVE
			if (si.hasLoop())
				n_ctxts[i] = ULONG_MAX;
#endif
			forall(vector<CallGraphNode *>, vi, *si) {
				// *vi is a (CallGraphNode *)
				// **vi is a CallGraphNode. 
				forall(CallGraphNode, it, **vi) {
					size_t j = node_to_scc[it->second];
					if (n_ctxts[i] == ULONG_MAX)
						n_ctxts[j] = ULONG_MAX;
					else
						n_ctxts[j] += n_ctxts[i];
				}
			}
		}
		return false;
	}

	void CountCtxts::print(raw_ostream &O, const Module *M) const {
		vector<pair<unsigned long, const Function *> > ans;
		forallconst(Module, fi, *M)
			ans.push_back(make_pair(num_ctxts(fi), fi));
		sort(ans.begin(), ans.end(),
				greater<pair<unsigned long, const Function *> >());
		for (size_t i = 0; i < ans.size(); ++i) {
			O << ans[i].second->getNameStr() << ": ";
			if (ans[i].first == ULONG_MAX)
				O << "oo";
			else
				O << ans[i].first;
			O << "\n";
		}
	}

	void CountCtxts::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequiredTransitive<CallGraphFP>(); // used in <num_ctxts>
		ModulePass::getAnalysisUsage(AU);
	}

	unsigned long CountCtxts::num_ctxts(const Function *f) const {
		const CallGraphFP &CG = getAnalysis<CallGraphFP>();
		const CallGraphNode *node = CG[f];
		assert(node);
		if (!node_to_scc.count(node)) {
			// Unreachable from main
			return 0;
		}
		size_t i = node_to_scc.lookup(node);
		assert(i < n_ctxts.size());
		return n_ctxts[i];
	}
}

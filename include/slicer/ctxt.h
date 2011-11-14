/**
 * Author: Jingyue
 */

#ifndef __SLICER_CTXT_H
#define __SLICER_CTXT_H

namespace slicer {

	/**
	 * Count how many calling contexts a function has. 
	 */
	struct CountCtxts: public ModulePass {

		static char ID;

		CountCtxts(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		unsigned long num_ctxts(const Function *f) const;

	private:
		// Indexed by the SCC ID. 
		vector<unsigned long> n_ctxts;
		DenseMap<const CallGraphNode *, size_t> node_to_scc;
	};
}

#endif

/**
 * Author: Jingyue
 *
 * Test only. Not a part of int-constraints. 
 */

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "common/include/util.h"
#include "common/cfg/partial-icfg-builder.h"
#include "idm/id.h"
using namespace llvm;


namespace slicer {

	struct TestICFG: public ModulePass {

		static char ID;

		TestICFG(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

namespace slicer {

	void TestICFG::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<MicroBasicBlockBuilder>();
		AU.addRequired<PartialICFGBuilder>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool TestICFG::runOnModule(Module &M) {
		ICFG &IM = getAnalysis<PartialICFGBuilder>();
		DominatorTreeBase<ICFGNode> DT(false);
		DT.recalculate((ICFG &)IM);
		assert(DT.getRootNode());
		MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
		ObjectID &OI = getAnalysis<ObjectID>();
		forallbb(M, bi) {
			for (mbb_iterator mi = MBBB.begin(bi); mi != MBBB.end(bi); ++mi) {
				ICFGNode *node = IM[mi];
				if (!node)
					continue;
				DomTreeNodeBase<ICFGNode> *dt_node = DT.getNode(node);
				if (!dt_node) {
					errs() << "[Warning] No DT node for MBB:" << mi->front() << "\n";
					continue;
				}
				if (DomTreeNodeBase<ICFGNode> *dt_idom = dt_node->getIDom()) {
					MicroBasicBlock *idom = dt_idom->getBlock()->getMBB();
					errs() << "idom: " << OI.getMicroBasicBlockID(idom) << " "
						<< OI.getMicroBasicBlockID(mi) << "\n";
				}
			}
		}
		return false;
	}

	char TestICFG::ID = 0;
}

namespace {

	static RegisterPass<slicer::TestICFG> X(
			"test-icfg",
			"Test ICFG",
			false,
			true);
}

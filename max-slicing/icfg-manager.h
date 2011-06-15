#ifndef __SLICER_ICFG_MANAGER_H
#define __SLICER_ICFG_MANAGER_H

#include "llvm/Pass.h"
#include "common/include/typedefs.h"
#include "common/cfg/icfg.h"
using namespace llvm;

namespace slicer {

	struct ICFGManager: public ModulePass, public ICFG {

		static char ID;

		ICFGManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		/** 
		 * This method is used when a pass implements
		 * an analysis interface through multiple inheritance.  If needed, it
		 * should override this to adjust the this pointer as needed for the
		 * specified pass info.
		 */
		virtual void *getAdjustedAnalysisPointer(const PassInfo *PI) {   
			if (PI->isPassID(&ICFG::ID))
				return (ICFG *)this;
			return this;
		}
	};
}

#endif

/**
 * Author: Jingyue
 */

#ifndef __SLICER_ASSERT_EQ_REMOVER_H
#define __SLICER_ASSERT_EQ_REMOVER_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct AssertEqRemover: public BasicBlockPass {
		static char ID;

		AssertEqRemover();
		virtual bool runOnBasicBlock(BasicBlock &BB);
	};
}

#endif

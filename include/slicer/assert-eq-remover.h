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

		AssertEqRemover():BasicBlockPass(&ID) {}
		virtual bool runOnBasicBlock(BasicBlock &BB);
	};
}

#endif

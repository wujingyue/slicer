/**
 * Author: Jingyue
 */

#include "slicer/capture.h"
#include "slicer/assert-eq-remover.h"
using namespace llvm;
using namespace slicer;

char AssertEqRemover::ID = 0;

AssertEqRemover::AssertEqRemover(): BasicBlockPass(ID) {
}

bool AssertEqRemover::runOnBasicBlock(BasicBlock &BB) {
	bool changed = false;

	BasicBlock::iterator ins = BB.begin();
	while (ins != BB.end()) {
		const Value *v = NULL;
		if (CaptureConstraints::is_slicer_assert_eq(ins, &v, NULL)) {
			if (isa<Constant>(v)) {
				BasicBlock::iterator next = ins; ++next;
				ins->eraseFromParent();
				ins = next;
				changed = true;
				continue;
			}
		}
		++ins;
	}

	return changed;
}

/**
 * Author: Jingyue
 *
 * Replace MyMalloc/MyFree with malloc/free. 
 */

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

namespace slicer {
	struct MyMalloc: public BasicBlockPass {
		static char ID;

		MyMalloc(): BasicBlockPass(ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnBasicBlock(BasicBlock &BB);
	};
}
using namespace slicer;

static RegisterPass<MyMalloc> X("replace-mymalloc",
		"Replace MyMalloc/MyFree with malloc/free");

char MyMalloc::ID = 0;

void MyMalloc::getAnalysisUsage(AnalysisUsage &AU) const {
	BasicBlockPass::getAnalysisUsage(AU);
}

bool MyMalloc::runOnBasicBlock(BasicBlock &BB) {
	bool changed = false;
	
	for (BasicBlock::iterator ins = BB.begin(); ins != BB.end(); ++ins) {
		CallSite cs(ins);
		if (cs.getInstruction()) {
			Function *callee = cs.getCalledFunction();
			if (callee && callee->getName() == "MyMalloc") {
				changed = true;
				assert(cs.arg_size() == 2);
				Value *size = cs.getArgument(0);
				const PointerType *ptr_type = dyn_cast<PointerType>(ins->getType());
				assert(ptr_type);
				Instruction *ci = CallInst::CreateMalloc(
						ins, size->getType(), ptr_type->getElementType(), size);
				ci->removeFromParent();
				if (ins->getType() != ci->getType()) {
					errs() << *ins << "\n";
					errs() << *ci << "\n";
				}
				assert(ins->getType() == ci->getType());
				ReplaceInstWithInst(ins, ci);
				ins = ci;
			}
			if (callee && callee->getName() == "MyFree") {
				changed = true;
				assert(cs.arg_size() == 1);
				CallInst::CreateFree(cs.getArgument(0), ins);
				BasicBlock::iterator ci = ins; --ci; // ci = the created free
				ci->removeFromParent();
				assert(ins->getType() == ci->getType());
				ReplaceInstWithInst(ins, ci);
				ins = ci;
			}
		}
	}

	return changed;
}

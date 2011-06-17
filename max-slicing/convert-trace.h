/**
 * Author: Jingyue
 *
 * Probably not useful anymore. 
 */

#ifndef __SLICER_CONVERT_TRACE_H
#define __SLICER_CONVERT_TRACE_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

namespace slicer {
	
	struct ConvertTrace: public ModulePass {

		static char ID;

		const static unsigned INVALID_IDX = (unsigned)(-1);

		ConvertTrace(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif

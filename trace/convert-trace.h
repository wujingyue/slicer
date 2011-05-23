#ifndef __TERN_CONVERT_TRACE_H
#define __TERN_CONVERT_TRACE_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

#include "trace-manager.h"
#include "trace.h"

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


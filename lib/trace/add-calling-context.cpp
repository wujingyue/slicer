#include <fstream>
using namespace std;

#include "llvm/Support/CommandLine.h"
#include "common/util.h"
#include "common/IDAssigner.h"
using namespace llvm;

#include "slicer/trace-manager.h"
#include "slicer/add-calling-context.h"
using namespace slicer;

char AddCallingContext::ID = 0;

static RegisterPass<slicer::AddCallingContext> X("add-calling-context",
		"Add a calling context for each instruction in the full trace",
		false, true); // is analysis

bool AddCallingContext::runOnModule(Module &M) {
	TraceManager &TM = getAnalysis<TraceManager>();
	// Last instruction of each thread. 
	DenseMap<int, unsigned> last_indices;

	contexts.clear();
	for (unsigned i = 0, E = TM.get_num_records(); i < E; ++i) {
		const TraceRecordInfo &record_info = TM.get_record_info(i);
		// Modify the callstack if it's a call or a ret.
		if (last_indices.count(record_info.tid)) {
			// Not the beginning of a thread. 
			unsigned last_idx = last_indices[record_info.tid];
			ConstInstList cur(contexts[last_idx]);
			const Instruction *last = TM.get_record_info(last_idx).ins;
			if (is_call(last) && is_function_entry(record_info.ins)) {
				cur.push_back(last);
			} else if (is_ret(last)) {
				assert(cur.size() > 0);
				cur.pop_back();
			}
			contexts.push_back(cur);
		} else {
			// The beginning of function main or thread functions. 
			contexts.push_back(ConstInstList());
		}
		// Update the last instruction. 
		last_indices[record_info.tid] = i;
	}
	return false;
}

void AddCallingContext::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<IDAssigner>();
	AU.addRequired<TraceManager>();
	ModulePass::getAnalysisUsage(AU);
}

ConstInstList AddCallingContext::get_calling_context(unsigned idx) const {
	assert(idx < contexts.size());
	return contexts[idx];
}

void AddCallingContext::print(raw_ostream &O, const Module *M) const {
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	for (size_t i = 0; i < contexts.size(); ++i) {
		errs() << i << ":";
		for (size_t j = 0; j < contexts[i].size(); ++j)
			errs() << " " << IDA.getInstructionID(contexts[i][j]);
		errs() << "\n";
	}
}

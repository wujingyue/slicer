#include "llvm/Support/CommandLine.h"
#include "common/include/util.h"
using namespace llvm;

#include <fstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "trace-manager.h"
#include "add-calling-context.h"

namespace {
	
	static RegisterPass<slicer::AddCallingContext> X(
			"tern-add-calling-context",
			"Add a calling context for each instruction in the full trace",
			false,
			true); // is analysis
}

namespace slicer {

	bool AddCallingContext::runOnModule(Module &M) {

		TraceManager &TM = getAnalysis<TraceManager>();
		// Last instruction of each thread. 
		map<int, unsigned> last_indices;
		// Thread creation site of each thread. 
		map<int, unsigned> created_by;

		contexts.clear();
		for (unsigned i = 0, E = TM.get_num_records(); i < E; ++i) {
			const TraceRecordInfo &record_info = TM.get_record_info(i);
			// Modify the callstack if it's a call or a ret.
			if (last_indices.count(record_info.tid)) {
				// Not the beginning of a thread. 
				unsigned last_idx = last_indices[record_info.tid];
				CallStack cur(contexts[last_idx]);
				Instruction *last = TM.get_record_info(last_idx).ins;
				if (is_call(last) && is_func_entry(record_info.ins)) {
					cur.push_back(last_idx);
				}
				if (isa<ReturnInst>(last) || isa<UnwindInst>(last)) {
					if (!cur.empty()) {
						cur.pop_back();
					} else {
						// May be ret from main or
						// ret from the static initialization function. 
						cerr << "[Warning] Calls and rets don't match: "
							<< "[" << record_info.tid << "]" << i << endl;
					}
				}
				contexts.push_back(cur);
			} else if (created_by.count(record_info.tid)) {
				// The beginning of a child thread. 
				unsigned creation_site = created_by[record_info.tid];
				CallStack cur(contexts[creation_site]);
				cur.push_back(creation_site);
				contexts.push_back(cur);
			} else {
				// The beginning of the main thread. 
				contexts.push_back(CallStack());
			}
			// Update the last instruction. 
			last_indices[record_info.tid] = i;
			if (record_info.child_tid != -1 &&
					record_info.child_tid != record_info.tid) {
				// A thread creation. 
				created_by[record_info.child_tid] = i;
			}
		}
		return false;
	}

	void AddCallingContext::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<TraceManager>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool AddCallingContext::is_func_entry(const Instruction *ins) {
		const BasicBlock *bb = ins->getParent();
		if (ins != bb->begin())
			return false;
		const Function *func = bb->getParent();
		if (bb != &func->getEntryBlock())
			return false;
		return true;
	}

	const CallStack &AddCallingContext::get_calling_context(unsigned idx) const {
		assert(idx < contexts.size());
		return contexts[idx];
	}

	void AddCallingContext::print(raw_ostream &O, const Module *M) const {
		for (size_t i = 0; i < contexts.size(); ++i) {
			cerr << i << ":";
			for (size_t j = 0; j < contexts[i].size(); ++j)
				cerr << " " << contexts[i][j];
			cerr << endl;
		}
	}

	char AddCallingContext::ID = 0;
}


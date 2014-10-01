#include "llvm/Support/CommandLine.h"
#include "rcs/IDAssigner.h"
#include "rcs/util.h"
using namespace llvm;

#include "slicer/query-gen.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
#include "slicer/enforcing-landmarks.h"
#include "slicer/landmark-trace.h"
#include "slicer/mark-landmarks.h"
#include "pointer-access.h"
using namespace slicer;

static cl::opt<bool> ForOriginalProgram("for-orig",
		cl::desc("Generate alias queries for the original program instead of the "
			"max-sliced program"));
static cl::opt<bool> Concurrent("concurrent",
		cl::desc("Consider concurrent instructions only"));
static cl::opt<bool> ContextSensitive("cs",
		cl::desc("Generate context sensitive queries"));
static cl::opt<bool> LoadLoad("gen-loadload",
		cl::desc("Generate load-load alias queries as well"));
static cl::opt<int> SampleRate("sample",
		cl::desc("Sample a subset of queries: 1/sample of all queries will "
			"be picked"),
		cl::init(1));

char QueryGenerator::ID = 0;

void QueryGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	// Used in print
	AU.addRequiredTransitive<IDAssigner>();
	if (Concurrent) {
		AU.addRequired<RegionManager>();
		AU.addRequired<EnforcingLandmarks>();
		AU.addRequired<MarkLandmarks>();
		AU.addRequired<LandmarkTrace>();
		AU.addRequired<TraceManager>();
	} else {
		// When Concurrent is true, the input bc is the original bc which
		// does not contain any clone_info. Therefore, we shouldn't require
		// CloneInfoManager in that situation.
		AU.addRequiredTransitive<CloneInfoManager>();
	}
}
QueryGenerator::QueryGenerator(): ModulePass(ID) {
}

void QueryGenerator::generate_static_queries(Module &M) {
	// Deterministic sampling to be fair.
	unsigned counter_for_sampling = 0;

	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();

	vector<Instruction *> read_accessors, write_accessors;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (CIM.has_clone_info(ins)) {
					vector<PointerAccess> accesses = get_pointer_accesses(ins);
					bool has_read = false, has_write = false;
					for (size_t i = 0; i < accesses.size(); ++i) {
						if (accesses[i].is_write)
							has_write = true;
						else
							has_read = true;
					}
					if (has_write)
						write_accessors.push_back(ins);
					if (has_read)
						read_accessors.push_back(ins);
				}
			}
		}
	}
	for (size_t i = 0; i < write_accessors.size(); ++i) {
		for (size_t j = i + 1; j < write_accessors.size(); ++j) {
			if ((++counter_for_sampling) % SampleRate != 0)
				continue;
			all_queries.push_back(make_pair(
						DynamicInstructionWithContext(-1, 0, write_accessors[i]),
						DynamicInstructionWithContext(-1, 0, write_accessors[j])));
		}
	}
	for (size_t i = 0; i < write_accessors.size(); ++i) {
		for (size_t j = 0; j < read_accessors.size(); ++j) {
			if ((++counter_for_sampling) % SampleRate != 0)
				continue;
			all_queries.push_back(make_pair(
						DynamicInstructionWithContext(-1, 0, write_accessors[i]),
						DynamicInstructionWithContext(-1, 0, read_accessors[j])));
		}
	}
	if (LoadLoad) {
		for (size_t i = 0; i < read_accessors.size(); ++i) {
			for (size_t j = i + 1; j < read_accessors.size(); ++j) {
				if ((++counter_for_sampling) % SampleRate != 0)
					continue;
				all_queries.push_back(make_pair(
							DynamicInstructionWithContext(-1, 0, read_accessors[i]),
							DynamicInstructionWithContext(-1, 0, read_accessors[j])));
			}
		}
	}

	errs() << "# of queries = " << counter_for_sampling << "\n";
}

void QueryGenerator::generate_dynamic_queries(Module &M) {
	// Deterministic sampling to be fair.
	unsigned counter_for_sampling = 0;

	TraceManager &TM = getAnalysis<TraceManager>();
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	RegionManager &RM = getAnalysis<RegionManager>();
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();

	DenseMap<Region, DenseSet<DynamicInstructionWithContext> > sls_in_regions;
	DenseMap<int, DenseSet<DynamicInstructionWithContext> > sls_in_cur_region;
	DenseMap<int, const Instruction *> last_inst;
	DenseMap<int, vector<DynamicInstruction> > last_callstack;
	DenseMap<int, size_t> last_enforcing, last_landmark;

	for (unsigned i = 0; i < TM.get_num_records(); ++i) {
		const TraceRecordInfo &info = TM.get_record_info(i);
		const Instruction *last_inst_of_the_thread = last_inst[info.tid];
		size_t last_landmark_of_the_thread = (size_t)-1;
		if (last_landmark.count(info.tid))
			last_landmark_of_the_thread = last_landmark[info.tid];

		if (last_inst_of_the_thread && is_call(last_inst_of_the_thread) &&
				is_function_entry(info.ins)) {
			assert(last_landmark_of_the_thread != (size_t)-1);
			last_callstack[info.tid].push_back(DynamicInstruction(
						info.tid, last_landmark_of_the_thread, last_inst_of_the_thread));
		} else if (last_inst_of_the_thread && is_ret(last_inst_of_the_thread)) {
			if (last_callstack[info.tid].size() == 0)
				errs() << "Error at Record " << i << "\n";
			assert(last_callstack[info.tid].size() > 0);
			BasicBlock::const_iterator ret_site = last_callstack[info.tid].back().ins;
			last_callstack[info.tid].pop_back();
			BasicBlock::const_iterator ins = ret_site;
			const BasicBlock *bb = ins->getParent();
			for (++ins; info.ins != ins && ins != bb->end(); ++ins) {
				if (!get_pointer_accesses(ins).empty()) {
					assert(last_landmark_of_the_thread != (size_t)-1);
					sls_in_cur_region[info.tid].insert(DynamicInstructionWithContext(
								info.tid, last_landmark_of_the_thread, ins,
								last_callstack[info.tid]));
				}
			}
		} else if (last_inst_of_the_thread) {
			const BasicBlock *bb = last_inst_of_the_thread->getParent();
			for (BasicBlock::const_iterator ins = last_inst_of_the_thread;
					info.ins != ins && ins != bb->end(); ++ins) {
				if (!get_pointer_accesses(ins).empty()) {
					assert(last_landmark_of_the_thread != (size_t)-1);
					sls_in_cur_region[info.tid].insert(DynamicInstructionWithContext(
								info.tid, last_landmark_of_the_thread, ins,
								last_callstack[info.tid]));
				}
			}
		}
		// Update <last_inst>.
		last_inst[info.tid] = info.ins;
		// Update <last_landmark>.
		if (ML.is_landmark(info.ins)) {
			size_t trunk_id = LT.search_landmark_in_thread(info.tid, i);
			// If enforcing landmark, flush sls
			last_landmark[info.tid] = trunk_id;
			if (EL.is_enforcing_landmark(info.ins)) {
				size_t last_enforcing_of_the_thread = (size_t)-1;
				if (last_enforcing.count(info.tid))
					last_enforcing_of_the_thread = last_enforcing[info.tid];
				Region cur_region(info.tid, last_enforcing_of_the_thread, trunk_id);
				sls_in_regions[cur_region] = sls_in_cur_region[info.tid];
				sls_in_cur_region[info.tid].clear();
				last_enforcing[info.tid] = trunk_id;
			}
		}
	}

	// Handle the last region of each thread.
	for (DenseMap<int, DenseSet<DynamicInstructionWithContext> >::iterator
			itr = sls_in_cur_region.begin(); itr != sls_in_cur_region.end(); ++itr) {
		if (itr->second.size() > 0) {
			size_t last_enforcing_of_the_thread = (size_t)-1;
			if (last_enforcing.count(itr->first))
				last_enforcing_of_the_thread = last_enforcing[itr->first];
			Region cur_region(itr->first, last_enforcing_of_the_thread, (size_t)-1);
			sls_in_regions[cur_region] = itr->second;
			itr->second.clear();
		}
	}

	// Look at each pair of concurrent regions.
	// TODO: Make it faster
	errs() << "# of regions = " << sls_in_regions.size() << "\n";
	for (DenseMap<Region, DenseSet<DynamicInstructionWithContext> >::iterator
			i1 = sls_in_regions.begin(); i1 != sls_in_regions.end(); ++i1) {
		DenseMap<Region, DenseSet<DynamicInstructionWithContext> >::iterator i2 = i1;
		if (!LoadLoad) {
			++i2;
		}
		for (; i2 != sls_in_regions.end(); ++i2) {
			if (LoadLoad || RM.concurrent(i1->first, i2->first)) {
				if (LoadLoad && i1->first.thr_id == i2->first.thr_id) {
					// Not interested if i1 and i2 are in the same thread.
					continue;
				}
				for (DenseSet<DynamicInstructionWithContext>::iterator
						j1 = i1->second.begin(); j1 != i1->second.end(); ++j1) {
					vector<PointerAccess> accesses1 = get_pointer_accesses(j1->di.ins);
					for (DenseSet<DynamicInstructionWithContext>::iterator
							j2 = i2->second.begin(); j2 != i2->second.end(); ++j2) {
						vector<PointerAccess> accesses2 = get_pointer_accesses(j2->di.ins);
						for (size_t k1 = 0; k1 < accesses1.size(); ++k1) {
							for (size_t k2 = 0; k2 < accesses2.size(); ++k2) {
								if (LoadLoad || racy(accesses1[k1], accesses2[k2])) {
									if ((++counter_for_sampling) % SampleRate != 0)
										continue;
									all_queries.push_back(make_pair(*j1, *j2));
								}
							}
						}
					}
				}
			}
		}
	}

	errs() << "# of queries = " << counter_for_sampling << "\n";

	// Count the number of accesses in each thread.
	DenseMap<int, unsigned> num_of_accesses_in_threads;
	for (DenseMap<Region, DenseSet<DynamicInstructionWithContext> >::iterator
			i1 = sls_in_regions.begin(); i1 != sls_in_regions.end(); ++i1) {
		num_of_accesses_in_threads[i1->first.thr_id] += i1->second.size();
	}
	for (DenseMap<int, unsigned>::iterator itr = num_of_accesses_in_threads.begin();
	     itr != num_of_accesses_in_threads.end();
	     ++itr) {
		if (itr->second != 0) {
			errs() << "Thread " << itr->first << ": " << itr->second << "\n";
		}
	}
}

bool QueryGenerator::runOnModule(Module &M) {
	if (!Concurrent) {
		assert(!ContextSensitive && "We don't generate context-sensitive "
				"queries for static instructions");
		generate_static_queries(M);
	} else {
		generate_dynamic_queries(M);
	}

	return false;
}

void QueryGenerator::print(raw_ostream &O, const Module *M) const {
	for (size_t i = 0; i < all_queries.size(); ++i) {
		print_dynamic_instruction_with_context(O, all_queries[i].first);
		O << ", ";
		print_dynamic_instruction_with_context(O, all_queries[i].second);
		O << "\n";
	}
}

void QueryGenerator::print_dynamic_instruction(raw_ostream &O,
		const DynamicInstruction &di) const {
	if (!Concurrent) {
		if (ForOriginalProgram) {
			CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
			assert(CIM.has_clone_info(di.ins));
			O << CIM.get_clone_info(di.ins).orig_ins_id;
		} else {
			IDAssigner &IDA = getAnalysis<IDAssigner>();
			unsigned ins_id = IDA.getInstructionID(di.ins);
			assert(ins_id != (unsigned)-1);
			O << ins_id;
		}
	} else {
		IDAssigner &IDA = getAnalysis<IDAssigner>();
		if (ForOriginalProgram) {
			O << IDA.getInstructionID(di.ins);
		} else {
			O << "(" << di.thread_id << ", " << di.trunk_id << ", "
				<< IDA.getInstructionID(di.ins) << ")";
		}
	}
}

void QueryGenerator::print_dynamic_instruction_with_context(raw_ostream &O,
		const DynamicInstructionWithContext &diwc) const {
	if (!ContextSensitive) {
		print_dynamic_instruction(O, diwc.di);
	} else {
		assert(Concurrent && "Not supported");
		for (size_t i = 0; i < diwc.callstack.size(); ++i) {
			print_dynamic_instruction(O, diwc.callstack[i]);
			O << " ";
		}
		print_dynamic_instruction(O, diwc.di);
	}
}

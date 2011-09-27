#include "llvm/Support/CommandLine.h"
#include "common/IDAssigner.h"
#include "common/util.h"
using namespace llvm;

#include "slicer/query-gen.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
#include "slicer/enforcing-landmarks.h"
#include "slicer/landmark-trace.h"
#include "slicer/mark-landmarks.h"
#include "slicer/add-calling-context.h"
using namespace slicer;

static RegisterPass<QueryGenerator> X("gen-queries",
		"Generate alias queries from a program",
		false, true);

static cl::opt<bool> ForOriginalProgram("for-orig",
		cl::desc("Generate alias queries for the original program instead of the "
			"max-sliced program"));
static cl::opt<bool> Concurrent("concurrent",
		cl::desc("Consider concurrent instructions only"));
static cl::opt<bool> ContextSensitive("cs",
		cl::desc("Generate context sensitive queries"));

char QueryGenerator::ID = 0;

void QueryGenerator::generate_static_queries(Module &M) {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();

	vector<StoreInst *> all_stores;
	vector<LoadInst *> all_loads;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (CIM.has_clone_info(ins)) {
					if (StoreInst *si = dyn_cast<StoreInst>(ins))
						all_stores.push_back(si);
					if (LoadInst *li = dyn_cast<LoadInst>(ins))
						all_loads.push_back(li);
				}
			}
		}
	}
	for (size_t i = 0; i < all_stores.size(); ++i) {
		for (size_t j = i + 1; j < all_stores.size(); ++j) {
			all_queries.push_back(make_pair(
						DynamicInstruction(all_stores[i], ConstInstList(), -1, 0),
						DynamicInstruction(all_stores[j], ConstInstList(), -1, 0)));
		}
	}
	for (size_t i = 0; i < all_stores.size(); ++i) {
		for (size_t j = 0; j < all_loads.size(); ++j) {
			all_queries.push_back(make_pair(
						DynamicInstruction(all_stores[i], ConstInstList(), -1, 0),
						DynamicInstruction(all_stores[j], ConstInstList(), -1, 0)));
		}
	}
}

void QueryGenerator::generate_dynamic_queries(Module &M) {
	TraceManager &TM = getAnalysis<TraceManager>();
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	RegionManager &RM = getAnalysis<RegionManager>();
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();

	DenseMap<Region, DenseSet<DynamicInstruction> > sls_in_regions;
	DenseMap<int, DenseSet<DynamicInstruction> > sls_in_cur_region;
	DenseMap<int, const Instruction *> last_inst;
	DenseMap<int, ConstInstList> last_callstack;
	DenseMap<int, size_t> last_enforcing, last_landmark;

	for (unsigned i = 0; i < TM.get_num_records(); ++i) {
		const TraceRecordInfo &info = TM.get_record_info(i);
		const Instruction *last_inst_of_the_thread = last_inst[info.tid];
		if (last_inst_of_the_thread && is_call(last_inst_of_the_thread) &&
				is_function_entry(info.ins)) {
			last_callstack[info.tid].push_back(last_inst_of_the_thread);
		} else if (last_inst_of_the_thread && is_ret(last_inst_of_the_thread)) {
			assert(last_callstack[info.tid].size() > 0);
			BasicBlock::const_iterator ret_site = last_callstack[info.tid].back();
			last_callstack[info.tid].pop_back();
			BasicBlock::const_iterator ins = ret_site;
			const BasicBlock *bb = ins->getParent();
			for (++ins; info.ins != ins && ins != bb->end(); ++ins) {
				if (isa<StoreInst>(ins) || isa<LoadInst>(ins)) {
					assert(last_landmark.count(info.tid));
					sls_in_cur_region[info.tid].insert(DynamicInstruction(ins,
								last_callstack[info.tid], info.tid, last_landmark[info.tid]));
				}
			}
		} else if (last_inst_of_the_thread) {
			const BasicBlock *bb = last_inst_of_the_thread->getParent();
			for (BasicBlock::const_iterator ins = last_inst_of_the_thread;
					info.ins != ins && ins != bb->end(); ++ins) {
				if (isa<StoreInst>(ins) || isa<LoadInst>(ins)) {
					assert(last_landmark.count(info.tid));
					sls_in_cur_region[info.tid].insert(DynamicInstruction(ins,
								last_callstack[info.tid], info.tid, last_landmark[info.tid]));
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
	for (DenseMap<int, DenseSet<DynamicInstruction> >::iterator
			itr = sls_in_cur_region.begin(); itr != sls_in_cur_region.end(); ++itr) {
		if (itr->second.size() > 0) {
			size_t last_enforcing_of_the_thread;
			if (last_enforcing.count(itr->first))
				last_enforcing_of_the_thread = last_enforcing[itr->first];
			else
				last_enforcing_of_the_thread = -1;
			Region cur_region(itr->first, last_enforcing_of_the_thread, (size_t)-1);
			sls_in_regions[cur_region] = itr->second;
			itr->second.clear();
		}
	}

	// Look at each pair of concurrent regions. 
	// TODO: Make it faster
	errs() << "# of regions = " << sls_in_regions.size() << "\n";
	for (DenseMap<Region, DenseSet<DynamicInstruction> >::iterator
			i1 = sls_in_regions.begin(); i1 != sls_in_regions.end(); ++i1) {
		DenseMap<Region, DenseSet<DynamicInstruction> >::iterator i2 = i1;
		for (++i2; i2 != sls_in_regions.end(); ++i2) {
			if (RM.concurrent(i1->first, i2->first)) {
				for (DenseSet<DynamicInstruction>::iterator j1 = i1->second.begin();
						j1 != i1->second.end(); ++j1) {
					for (DenseSet<DynamicInstruction>::iterator j2 = i2->second.begin();
							j2 != i2->second.end(); ++j2) {
						unsigned n_stores = 0;
						if (isa<StoreInst>(j1->ins))
							++n_stores;
						if (isa<StoreInst>(j2->ins))
							++n_stores;
						if (n_stores >= 1)
							all_queries.push_back(make_pair(*j1, *j2));
					}
				}
			}
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
		print_dynamic_instruction(O, all_queries[i].first);
		O << ", ";
		print_dynamic_instruction(O, all_queries[i].second);
		O << "\n";
	}
}

void QueryGenerator::print_dynamic_instruction(raw_ostream &O,
		const DynamicInstruction &di) const {
	if (!Concurrent) {
		assert(!ContextSensitive && "Not supported");
		if (ForOriginalProgram) {
			CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
			assert(CIM.has_clone_info(di.ins));
			O << CIM.get_clone_info(di.ins).orig_ins_id;
		} else {
			IDAssigner &IDA = getAnalysis<IDAssigner>();
			O << IDA.getInstructionID(di.ins);
		}
	} else {
		IDAssigner &IDA = getAnalysis<IDAssigner>();
		if (ForOriginalProgram) {
			O << IDA.getInstructionID(di.ins);
		} else {
			O << "(" << di.thread_id << ", " << di.trunk_id << ", "
				<< IDA.getInstructionID(di.ins) << ")";
		}
		if (ContextSensitive) {
			O << " (";
			for (size_t i = 0; i < di.callstack.size(); ++i) {
				O << IDA.getInstructionID(di.callstack[i]);
				if (i + 1 < di.callstack.size())
					O << ", ";
			}
			O << ")";
		}
	}
}

void QueryGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	if (Concurrent) {
		AU.addRequired<RegionManager>();
		AU.addRequired<EnforcingLandmarks>();
		AU.addRequired<MarkLandmarks>();
		AU.addRequired<LandmarkTrace>();
		AU.addRequired<TraceManager>();
	}
	// Used in print
	if (!Concurrent) {
		// When Concurrent is true, the input bc is the original bc which
		// does not contain any clone_info. Therefore, we shouldn't require
		// CloneInfoManager in that situation. 
		AU.addRequiredTransitive<CloneInfoManager>();
	}
	AU.addRequiredTransitive<IDAssigner>();
	ModulePass::getAnalysisUsage(AU);
}

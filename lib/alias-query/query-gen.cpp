#include "llvm/Support/CommandLine.h"
#include "common/IDAssigner.h"
#include "common/util.h"
using namespace llvm;

#include "slicer/query-gen.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
#include "slicer/enforcing-landmarks.h"
#include "slicer/landmark-trace.h"
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
		for (size_t j = i + 1; j < all_stores.size(); ++j)
			all_queries.push_back(make_pair(all_stores[i], all_stores[j]));
	}
	for (size_t i = 0; i < all_stores.size(); ++i) {
		for (size_t j = 0; j < all_loads.size(); ++j)
			all_queries.push_back(make_pair(all_stores[i], all_loads[j]));
	}
}

void QueryGenerator::generate_dynamic_queries(Module &M) {
	typedef pair<const Instruction *, ConstInstList> InstContextPair;

	TraceManager &TM = getAnalysis<TraceManager>();
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	RegionManager &RM = getAnalysis<RegionManager>();

	DenseMap<Region, vector<InstContextPair> > sls_in_regions;
	DenseMap<int, const Instruction *> cur_inst;
	DenseMap<int, ConstInstList> cur_callstack;
	DenseMap<int, size_t> prev_enforcing;
	DenseMap<int, vector<InstContextPair> > sls_in_cur_region;
	for (unsigned i = 0; i < TM.get_num_records(); ++i) {
		const TraceRecordInfo &info = TM.get_record_info(i);
		const Instruction *cur_inst_of_the_thread = cur_inst[info.tid];
		if (is_call(cur_inst_of_the_thread) && is_function_entry(info.ins)) {
			cur_callstack[info.tid].push_back(cur_inst_of_the_thread);
		} else if (is_ret(cur_inst_of_the_thread)) {
			assert(cur_callstack[info.tid].size() > 0);
			BasicBlock::const_iterator ret_site = cur_callstack[info.tid].back();
			cur_callstack[info.tid].pop_back();
			BasicBlock::const_iterator ins = ret_site;
			for (++ins; ins != ins->getParent()->end(); ++ins) {
				if (isa<StoreInst>(ins) || isa<LoadInst>(ins))
					sls_in_cur_region[info.tid].push_back(
							make_pair(ins, cur_callstack[info.tid]));
			}
		} else {
			for (BasicBlock::const_iterator ins = cur_inst_of_the_thread;
					info.ins != ins; ++ins) {
				if (isa<StoreInst>(ins) || isa<LoadInst>(ins))
					sls_in_cur_region[info.tid].push_back(
							make_pair(ins, cur_callstack[info.tid]));
			}
		}
		// Update <cur_inst>.
		cur_inst[info.tid] = info.ins;
		// If enforcing landmark, flush sls
		if (EL.is_enforcing_landmark(info.ins)) {
			size_t prev_enforcing_of_the_thread;
			if (prev_enforcing.count(info.tid))
				prev_enforcing_of_the_thread = prev_enforcing[info.tid];
			else
				prev_enforcing_of_the_thread = -1;
			size_t trunk_id = LT.search_landmark_in_thread(info.tid, i);
			Region cur_region(info.tid, prev_enforcing_of_the_thread, trunk_id);
			sls_in_regions[cur_region] = sls_in_cur_region[info.tid];
			sls_in_cur_region[info.tid].clear();
			prev_enforcing[info.tid] = trunk_id;
		}
	}

	// Handle the last region of each thread. 
	for (DenseMap<int, vector<InstContextPair> >::iterator
			itr = sls_in_cur_region.begin(); itr != sls_in_cur_region.end(); ++itr) {
		if (itr->second.size() > 0) {
			size_t prev_enforcing_of_the_thread;
			if (prev_enforcing.count(itr->first))
				prev_enforcing_of_the_thread = prev_enforcing[itr->first];
			else
				prev_enforcing_of_the_thread = -1;
			Region cur_region(itr->first, prev_enforcing_of_the_thread, (size_t)-1);
			sls_in_regions[cur_region] = itr->second;
			itr->second.clear();
		}
	}

	// Look at each pair of concurrent regions. 
	// TODO: Make it faster
	for (DenseMap<Region, vector<InstContextPair> >::iterator
			i1 = sls_in_regions.begin(); i1 != sls_in_regions.end(); ++i1) {
		DenseMap<Region, vector<InstContextPair> >::iterator i2 = i1;
		for (++i2; i2 != sls_in_regions.end(); ++i2) {
			if (RM.concurrent(i1->first, i2->first)) {
				for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
					for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
						unsigned n_stores = 0;
						if (isa<StoreInst>(i1->second[j1].first))
							++n_stores;
						if (isa<StoreInst>(i2->second[j2].first))
							++n_stores;
						if (n_stores >= 1) {
							all_queries.push_back(make_pair(
										i1->second[j1].first,
										i2->second[j2].first));
						}
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

unsigned QueryGenerator::get_instruction_id(const Instruction *ins) const {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	if (ForOriginalProgram) {
		assert(CIM.has_clone_info(ins));
		return CIM.get_clone_info(ins).orig_ins_id;
	} else {
		return IDA.getInstructionID(ins);
	}
}

void QueryGenerator::print(raw_ostream &O, const Module *M) const {
	for (size_t i = 0; i < all_queries.size(); ++i) {
		O << get_instruction_id(all_queries[i].first) << " "
			<< get_instruction_id(all_queries[i].second) << "\n";
	}
}

void QueryGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<RegionManager>();
	AU.addRequired<EnforcingLandmarks>();
	AU.addRequired<LandmarkTrace>();
	// Used in print
	AU.addRequiredTransitive<CloneInfoManager>();
	AU.addRequiredTransitive<IDAssigner>();
	ModulePass::getAnalysisUsage(AU);
}

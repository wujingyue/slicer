/**
 * Author: Jingyue
 *
 * TODO: Save space and time by building the control flow graph
 * on MicroBasicBlocks instead of Instructions. 
 */

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "common/id-manager/IDManager.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/may-exec.h"
using namespace llvm;

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "config.h"
#include "max-slicing.h"
#include "trace/trace-manager.h"
#include "trace/mark-landmarks.h"
#include "trace/landmark-trace.h"
using namespace slicer;

static RegisterPass<MaxSlicing> X(
		"max-slicing",
		"Slice and unroll the program according to the trace",
		false,
		false); // max-slicing is a pure transform pass. 

void MaxSlicing::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<IDManager>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<CallGraphFP>();
	AU.addRequired<MayExec>();
	AU.addRequired<TraceManager>();
	AU.addRequired<MarkLandmarks>();
	AU.addRequired<LandmarkTrace>();
	ModulePass::getAnalysisUsage(AU);
}

void MaxSlicing::print_inst_set(const InstSet &s) {
	IDManager &IDM = getAnalysis<IDManager>();
	vector<unsigned> inst_ids;
	for (InstSet::const_iterator it = s.begin();
			it != s.end(); ++it)
		inst_ids.push_back(IDM.getInstructionID(*it));
	sort(inst_ids.begin(), inst_ids.end());
	forall(vector<unsigned>, it, inst_ids)
		cerr << *it << " ";
	cerr << endl;
}

void MaxSlicing::print_edge_set(const EdgeSet &s) {
	IDManager &IDM = getAnalysis<IDManager>();
	vector<pair<unsigned, unsigned> > edge_ids;
	forallconst(EdgeSet, it, s) {
		edge_ids.push_back(make_pair(
					IDM.getInstructionID(it->first),
					IDM.getInstructionID(it->second)));
	}
	sort(edge_ids.begin(), edge_ids.end());
	for (size_t i = 0; i < edge_ids.size(); ++i)
		cerr << "(" << edge_ids[i].first << "," << edge_ids[i].second << ") ";
	cerr << endl;
}

void MaxSlicing::print_levels_in_thread(
		int thr_id,
		const DenseMap<Instruction *, int> &level) {
	vector<pair<pair<size_t, unsigned>, int> > leveled;
	for (size_t i = 0; i < clone_map[thr_id].size(); ++i) {
		forall(InstMapping, it, clone_map[thr_id][i]) {
			IDManager &IDM = getAnalysis<IDManager>();
			if (level.count(it->second)) {
				leveled.push_back(make_pair(
							make_pair(i, IDM.getInstructionID(it->first)),
							level.lookup(it->second)));
			}
		}
	}
	sort(leveled.begin(), leveled.end());
	for (size_t i = 0; i < leveled.size(); ++i) {
		cerr << "{" << leveled[i].first.first << ":"
			<< leveled[i].first.second << "} = " << leveled[i].second << endl;
	}
}

void MaxSlicing::read_trace_and_cut(
		Trace &trace,
		vector<ThreadCreationRecord> &thr_cr_records,
		InstSet &cut) {

	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();
	TraceManager &TM = getAnalysis<TraceManager>();
	// cut
	cut = ML.get_landmarks();
	// trace and thr_cr_records
	const vector<int> &thr_ids = LT.get_thr_ids();
	for (size_t i = 0; i < thr_ids.size(); ++i) {
		int thr_id = thr_ids[i];
		const vector<unsigned> &thr_indices = LT.get_thr_trunks(thr_id);
		for (size_t j = 0; j < thr_indices.size(); ++j) {
			const TraceRecordInfo &info = TM.get_record_info(thr_indices[j]);
			trace[thr_id].push_back(info.ins);
			if (info.child_tid != -1 && info.child_tid != thr_id) {
				thr_cr_records.push_back(
						ThreadCreationRecord(thr_id, j, info.child_tid));
			}
		}
	}
}

bool MaxSlicing::runOnModule(Module &M) {
	// Make sure the original program has required ID information. 
	assert(getAnalysis<IDManager>().size() > 0 &&
			"The program does not have ID information.");
	// Read the trace and the cut. 
	Trace trace;
	vector<ThreadCreationRecord> thr_cr_records;
	InstSet cut;
	read_trace_and_cut(trace, thr_cr_records, cut);
	// Which functions may execute a landmark? 
	MayExec &ME = getAnalysis<MayExec>();
	ME.setup_landmarks(cut);
	ME.run();
	// Build the control flow graph. 
	// Output to <cfg>. 
	build_cfg(M, trace, cut);
	// Fix the def-use graph. 
	fix_def_use(M, trace);
	// Statistic. 
	stat(M);
	// Link thread functions. 
	link_thr_funcs(M, trace, thr_cr_records);
	// Redirect the program entry to main.SLICER. 
	assert(trace.count(0) && trace[0].size() > 0);
	Instruction *old_start = trace[0][0];
	assert(clone_map.count(0) && clone_map[0].size() > 0);
	Instruction *new_start = clone_map[0][0].lookup(old_start);
	assert(new_start && "Cannot find the program entry in the cloned CFG");
	// Remove all instructions in main, and add a call to main.SLICER instead. 
	// Note that this will invalidate some entries in <clone_map> and
	// <clone_map_r>. 
	clone_map.clear();
	clone_map_r.clear();
	cloned_to_tid.clear();
	cloned_to_trunk.clear();
	redirect_program_entry(old_start, new_start);
#ifdef VERBOSE
	cerr << "Dumping module...\n";
	M.dump();
#endif
	return true;
}

void MaxSlicing::stat(Module &M) {
	cerr << "Stat...\n";
	unsigned n_orig_insts = 0;
	InstSet cloned_orig_insts;
	forallinst(M, ii) {
		Instruction *orig = clone_map_r.lookup(ii);
		if (!orig)
			n_orig_insts++;
		else
			cloned_orig_insts.insert(orig);
	}
	cerr << cloned_orig_insts.size() << " out of " << n_orig_insts
		<< " instructions are still in the sliced program.\n";
}

void MaxSlicing::print_cloned_inst(Instruction *ins) {
	assert(clone_map_r.count(ins));
	assert(cloned_to_trunk.count(ins));
	Instruction *ii = clone_map_r.lookup(ins);
	cerr << "===== print_cloned_inst =====\n";
	cerr << ii->getParent()->getParent()->getNameStr() << "."
		<< ii->getParent()->getNameStr() << endl;
	ii->dump();
	IDManager &IDM = getAnalysis<IDManager>();
	cerr << "trunk = " << cloned_to_trunk.lookup(ins) << "; "
		<< "ID = " << IDM.getInstructionID(ii) << endl;
}

char MaxSlicing::ID = 0;

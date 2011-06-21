/**
 * Author: Jingyue
 *
 * TODO: Save space and time by building the control flow graph
 * on MicroBasicBlocks instead of Instructions. 
 */

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
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

/* Instructions that are not cloned will be contained in Thread -1 */
static cl::opt<string> MappingFile(
		"output-clone-map",
		cl::NotHidden,
		cl::desc("Output file containing the clone mapping"),
		cl::init(""));

static cl::opt<string> CFGFile(
		"output-cfg",
		cl::NotHidden,
		cl::desc("Ooutput file containing the CFG of the cloned program"),
		cl::init(""));

void MaxSlicing::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<ObjectID>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<CallGraphFP>();
	AU.addRequired<MayExec>();
	AU.addRequired<TraceManager>();
	AU.addRequired<MarkLandmarks>();
	AU.addRequired<LandmarkTrace>();
	ModulePass::getAnalysisUsage(AU);
}

void MaxSlicing::print_inst_set(const InstSet &s) {
	ObjectID &IDM = getAnalysis<ObjectID>();
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
	ObjectID &IDM = getAnalysis<ObjectID>();
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
			ObjectID &IDM = getAnalysis<ObjectID>();
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
	// Save the old id mapping before everything. 
	ObjectID &IDM = getAnalysis<ObjectID>();
	unsigned n_insts = IDM.getNumInstructions();
	for (unsigned i = 0; i < n_insts; ++i) {
		assert(IDM.getInstruction(i));
		old_id_map[IDM.getInstruction(i)] = i;
	}
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
	// Stat before the clone mappings become invalid.
	stat(M);
	// Link thread functions. 
	link_thr_funcs(M, trace, thr_cr_records);
	// <redirect_program_entry> invalidates <clone_map>.
	// Therefore, we build <clone_id_map> before that. 
	build_clone_id_map();
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
	old_id_map.clear();
	redirect_program_entry(old_start, new_start);
#ifdef VERBOSE
	cerr << "Dumping module...\n";
	M.dump();
#endif
	// Print auxiliary stuff. 
	print_aux(M);
	return true;
}

void MaxSlicing::print_aux(Module &M) const {
	// Rerun ObjectID and MicroBasicBlockBuilder to get consistent info. 
	bool modified;
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	modified = MBBB.runOnModule(M);
	assert(!modified && "MicroBasicBlockBuilder shouldn't modify the module.");
	// ObjectID requires MicroBasicBlockBuilder, thus we rerun MBBB first. 
	ObjectID &OI = getAnalysis<ObjectID>();
	modified = OI.runOnModule(M);
	assert(!modified && "ObjectID shouldn't modify the module.");
	// Print the ID mapping. 
	print_mapping(M);
	// Print the CFG of the cloned program. 
	print_cfg(M);
}

void MaxSlicing::build_clone_id_map() {
	for (map<int, vector<InstMapping> >::iterator it = clone_map.begin();
			it != clone_map.end(); ++it) {
		int thr_id = it->first;
		for (size_t tr_id = 0; tr_id < it->second.size(); ++tr_id) {
			DenseMap<unsigned, Instruction *> trunk_id_map;
			forall(InstMapping, j, it->second[tr_id]) {
				Instruction *orig = j->first;
				Instruction *cloned = j->second;
				assert(old_id_map.count(orig));
				trunk_id_map[old_id_map.lookup(orig)] = cloned;
			}
			clone_id_map[thr_id].push_back(trunk_id_map);
		}
		assert(clone_id_map[thr_id].size() == clone_map[thr_id].size());
	}
	// So that clone_id_map[-1][0] is valid. 
	clone_id_map[-1].push_back(DenseMap<unsigned, Instruction *>());
	for (DenseMap<Instruction *, unsigned>::iterator it = old_id_map.begin();
			it != old_id_map.end(); ++it) {
		clone_id_map[-1][0][it->second] = it->first;
	}
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
	ObjectID &IDM = getAnalysis<ObjectID>();
	cerr << "trunk = " << cloned_to_trunk.lookup(ins) << "; "
		<< "ID = " << IDM.getInstructionID(ii) << endl;
}

void MaxSlicing::print_cfg(Module &M) const {
	if (CFGFile == "")
		return;
	ObjectID &OI = getAnalysis<ObjectID>();
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	ofstream fout(CFGFile.c_str());
	forallbb(M, bi) {
		for (mbb_iterator mi = MBBB.begin(bi); mi != MBBB.end(bi); ++mi) {
			forall(MicroBasicBlock, ii, *mi) {
				if (mi->getParent()->getTerminator() != ii) {
					InstList nbrs = cfg.lookup(ii);
					assert(nbrs.size() == 0 || nbrs.size() == 1);
					for (size_t j = 0; j < nbrs.size(); ++j)
						assert(MBBB.parent(ii) == MBBB.parent(nbrs[j]));
				}
			}
			Instruction *last = mi->getLast();
			if (cfg.count(last)) {
				unsigned id_x = OI.getMicroBasicBlockID(mi);
				InstList nbrs = cfg.lookup(last);
				fout << id_x << ":";
				for (size_t j = 0; j < nbrs.size(); ++j) {
					MicroBasicBlock *y = MBBB.parent(nbrs[j]);
					unsigned id_y = OI.getMicroBasicBlockID(y);
					fout << " " << id_y;
				}
				fout << endl;
			}
		}
	}
}

void MaxSlicing::print_mapping(Module &M) const {
	if (MappingFile == "")
		return;
	ofstream fout(MappingFile.c_str());
	map<int, vector<DenseMap<unsigned, Instruction *> > >::const_iterator it;
	for (it = clone_id_map.begin(); it != clone_id_map.end(); ++it) {
		int thr_id = it->first;
		for (size_t tr_id = 0; tr_id < it->second.size(); ++tr_id) {
			DenseMap<unsigned, Instruction *>::const_iterator j;
			for (j = it->second[tr_id].begin();
					j != it->second[tr_id].end(); ++j) {
				unsigned old_id = j->first;
				Instruction *cloned = j->second;
				unsigned new_id = getAnalysis<ObjectID>().getInstructionID(cloned);
				assert(new_id != ObjectID::INVALID_ID);
				fout << thr_id << " "
					<< tr_id << " "
					<< old_id << " "
					<< new_id << "\n";
			}
		}
	}
}

char MaxSlicing::ID = 0;

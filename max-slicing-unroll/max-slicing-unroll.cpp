/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"

#include "config.h"
#include "max-slicing-unroll.h"
#include "idm/id.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/may-exec/may-exec.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace llvm;
using namespace std;

namespace {
	static RegisterPass<slicer::MaxSlicingUnroll>
		X("max-slicing-unroll",
				"Unroll the program according to the trace",
				false,
				true); // is analysis
	static cl::opt<string> TraceFile(
			"trace",
			cl::NotHidden,
			cl::desc("A trace of instructions according to which we will slice"
				" the program"),
			cl::init(""));
	static cl::opt<string> CutFile(
			"cut",
			cl::NotHidden,
			cl::desc("Set of instructions that should be traced"),
			cl::init(""));
}

namespace slicer {

	void MaxSlicingUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<ObjectID>();
		AU.addRequired<CallGraphFP>();
		AU.addRequired<MayExec>();
		ModulePass::getAnalysisUsage(AU);
	}

	MaxSlicingUnroll::MaxSlicingUnroll(): ModulePass(&ID) {}

	int MaxSlicingUnroll::read_trace(
			const string &trace_file,
			Trace &trace,
			vector<ThreadCreationRecord> &thr_cr_records) {
		if (trace_file == "") {
			cerr << "Didn't specify the trace file. Do nothing.\n";
			return -1;
		}
		ifstream fin(trace_file.c_str());
		if (!fin) {
			cerr << "Cannot open file " << trace_file << endl;
			return -1;
		}

		trace.clear();
		thr_cr_records.clear();
		string line;
		ObjectID &IDM = getAnalysis<ObjectID>();

		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id, ins_id, child_tid;
			if (iss >> thr_id >> ins_id >> child_tid) {
				Instruction *ins = IDM.getInstruction(ins_id);
				if (ins == NULL) {
					cerr << "Cannot find the instruction with ID " << ins_id << endl;
					return -1;
				}
				trace[thr_id].push_back(ins);
				if (child_tid != -1) {
					// A thread creation
					thr_cr_records.push_back(
							ThreadCreationRecord(
								thr_id,
								trace[thr_id].size() - 1,
								child_tid));
				}
			}
		}
		
		forall(Trace, it, trace) {
			if (it->second.size() < 2) {
				cerr << "There should be at least two entries in a trace\n";
				return -1;
			}
		}
		
		return 0;
	}

	int MaxSlicingUnroll::read_cut(
			const string &cut_file,
			InstSet &cut) {
		if (cut_file == "") {
			cerr << "Didn't specify the cut file. Do nothing.\n";
			return -1;
		}
		ifstream fin(cut_file.c_str());
		string line;
		ObjectID &IDM = getAnalysis<ObjectID>();
		cut.clear();
		while (getline(fin, line)) {
			istringstream iss(line);
			int i;
			if (iss >> i) {
				Instruction *ins = IDM.getInstruction(i);
				if (ins == NULL) {
					cerr << "Cannot find the instruction with ID " << i << endl;
					return -1;
				}
				cut.insert(ins);
			}
		}
		return 0;
	}

	int MaxSlicingUnroll::read_trace_and_cut(
			const string &trace_file,
			const string &cut_file,
			Trace &trace,
			vector<ThreadCreationRecord> &thr_cr_records,
			InstSet &cut) {
		// Read the trace. 
		if (read_trace(trace_file, trace, thr_cr_records))
			return -1;
		// Collect all instructions that should be recorded once executed. 
		if (read_cut(cut_file, cut))
			return -1;
		// <cut> should also contain all instructions in the trace. 
		for (Trace::iterator it = trace.begin();
				it != trace.end(); ++it) {
			for (size_t i = 0, E = it->second.size(); i < E; ++i)
				cut.insert(it->second[i]);
		}
		assert(!trace.empty() && "A trace cannot be empty");
		return 0;
	}

	void MaxSlicingUnroll::print_inst_set(const InstSet &s) {
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

	void MaxSlicingUnroll::print_edge_set(const EdgeSet &s) {
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

	void MaxSlicingUnroll::print_levels_in_thread(
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

	bool MaxSlicingUnroll::runOnModule(Module &M) {
		// Read the trace and the cut. 
		Trace trace;
		vector<ThreadCreationRecord> thr_cr_records;
		InstSet cut;
		if (read_trace_and_cut(TraceFile, CutFile, trace, thr_cr_records, cut))
			return false;

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

#if 0
		// Redirect the program entry to main.TERN. 
		// Remove all instructions in main, and add a call to main.TERN instead. 
		// Note that this will invalidate some entries in <clone_map> and
		// <clone_map_r>. 
		assert(trace.count(0));
		redirect_program_entry(M, trace[0][0]);
#endif
		// Link thread functions. 
		link_thr_funcs(M, trace, thr_cr_records);

#ifdef VERBOSE
		cerr << "Dumping module...\n";
		M.dump();
#endif

		bool modified = getAnalysis<ObjectID>().runOnModule(M);
		assert(!modified && "ObjectID shouldn't modify the module");

		return true;
	}

	void MaxSlicingUnroll::stat(Module &M) {
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

	void MaxSlicingUnroll::print_cloned_inst(Instruction *ins) {
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

	void MaxSlicingUnroll::print(raw_ostream &O, const Module *M) const {
		ObjectID &IDM = getAnalysis<ObjectID>();
		forallconst(InstMapping, it, clone_map_r) {
			Instruction *cloned = it->first;
			Instruction *orig = it->second;
			int thr_id = cloned_to_tid.lookup(cloned);
			unsigned trunk_id = cloned_to_trunk.lookup(cloned);
			O << thr_id << " "
				<< trunk_id << " "
				<< IDM.getInstructionID(orig) << " "
				<< IDM.getInstructionID(cloned) << "\n";
		}
	}

	Instruction *MaxSlicingUnroll::get_cloned_inst(
			int thr_id,
			unsigned trunk_id,
			Instruction *orig) const {
		if (!clone_map.count(thr_id))
			return NULL;
		const vector<InstMapping> &clone_map_in_thread =
			clone_map.find(thr_id)->second;
		if (trunk_id >= clone_map_in_thread.size())
			return NULL;
		const InstMapping &clone_map_in_trunk = clone_map_in_thread[trunk_id];
		return clone_map_in_trunk.lookup(orig);
	}

	Instruction *MaxSlicingUnroll::get_orig_inst(Instruction *cloned) const {
		// Returns NULL on NULL. 
		return clone_map_r.lookup(cloned);
	}
	
	char MaxSlicingUnroll::ID = 0;
}


/**
 * Author: Jingyue
 *
 * TODO: Save space and time by building the control flow graph
 * on MicroBasicBlocks instead of Instructions. 
 */

#define DEBUG_TYPE "max-slicing"

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "common/IDManager.h"
#include "common/callgraph-fp.h"
#include "common/exec.h"
using namespace llvm;

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "max-slicing.h"
#include "trace/landmark-trace.h"
#include "trace/enforcing-landmarks.h"
#include "trace/mark-landmarks.h"
using namespace slicer;

static RegisterPass<MaxSlicing> X("max-slicing",
		"Slice and unroll the program according to the trace");

STATISTIC(NumOrigInstructions, "Number of original instructions");
STATISTIC(NumOrigInstructionsLeft,
		"Number of original instructions left after slicing");
STATISTIC(NumInstructionsInSliced,
		"Number of all instructions in the sliced program");

void MaxSlicing::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<IDManager>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<CallGraphFP>();
	AU.addRequired<Exec>();
	AU.addRequired<MarkLandmarks>();
	AU.addRequired<EnforcingLandmarks>();
	AU.addRequired<LandmarkTrace>();
	AU.addRequired<DominatorTree>();
	ModulePass::getAnalysisUsage(AU);
}

void MaxSlicing::print_inst_set(raw_ostream &O, const InstSet &s) {
	IDManager &IDM = getAnalysis<IDManager>();
	vector<unsigned> inst_ids;
	for (InstSet::const_iterator it = s.begin();
			it != s.end(); ++it)
		inst_ids.push_back(IDM.getInstructionID(*it));
	sort(inst_ids.begin(), inst_ids.end());
	forall(vector<unsigned>, it, inst_ids)
		O << *it << " ";
	O << "\n";
}

void MaxSlicing::print_edge_set(raw_ostream &O, const EdgeSet &s) {
	IDManager &IDM = getAnalysis<IDManager>();
	vector<pair<unsigned, unsigned> > edge_ids;
	forallconst(EdgeSet, it, s) {
		edge_ids.push_back(make_pair(
					IDM.getInstructionID(it->first),
					IDM.getInstructionID(it->second)));
	}
	sort(edge_ids.begin(), edge_ids.end());
	for (size_t i = 0; i < edge_ids.size(); ++i)
		O << "(" << edge_ids[i].first << "," << edge_ids[i].second << ") ";
	O << "\n";
}

void MaxSlicing::read_trace_and_landmarks() {
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();
	IDManager &IDM = getAnalysis<IDManager>();
	
	// Get all the landmarks. 
	landmarks = ML.get_landmarks();

	// Read the entire trace. 
	trace.clear();
	const vector<int> &thr_ids = LT.get_thr_ids();
	for (size_t i = 0; i < thr_ids.size(); ++i) {
		int thr_id = thr_ids[i];
		for (unsigned trunk_id = 0; trunk_id < LT.get_n_trunks(thr_id); ++trunk_id) {
			const LandmarkTraceRecord &record = LT.get_landmark(thr_id, trunk_id);
			Instruction *ins = IDM.getInstruction(record.ins_id);
			if (!ins) {
				errs() << "ins_id = " << record.ins_id << "\n";
			}
			assert(ins && "Cannot find an instruction with this instruction ID");
			trace[thr_id].push_back(ins);
		}
	}
}

bool MaxSlicing::runOnModule(Module &M) {
	IDManager &IDM = getAnalysis<IDManager>();
	Exec &EXE = getAnalysis<Exec>();

	// Make sure the original program has required ID information. 
	assert(IDM.size() > 0 && "The program does not have ID information.");
	NumOrigInstructions = IDM.size();
	
	// Read the trace and the cut. 
	read_trace_and_landmarks();
	// Which functions may execute a landmark? 
	ConstInstSet const_landmarks;
	for (InstSet::iterator itr = landmarks.begin(); itr != landmarks.end();
			++itr) {
		const_landmarks.insert(*itr);
	}
	EXE.setup_landmarks(const_landmarks);
	EXE.run();
	
	// Build the control flow graph. 
	// Output to <cfg>. 
	build_cfg(M);
	
	// Fix the def-use graph. 
	fix_def_use(M);
	
	// Link thread functions. 
	link_thr_funcs(M);
	
	/*
	 * We support partial slicing. Even if the trace of the main thread is
	 * missing, we are still able to clone thread functions. 
	 */
	if (trace.count(0)) {
		// Redirect the program entry to main.SLICER. 
		assert(trace[0].size() > 0);
		Instruction *old_start = trace[0][0];
		assert(clone_map.count(0) && clone_map[0].size() > 0);
		Instruction *new_start = clone_map[0][0].lookup(old_start);
		assert(new_start && "Cannot find the program entry in the cloned CFG");
		redirect_program_entry(old_start, new_start);
	}

	// Finally, we mark all enforcing landmarks in the cloned program
	// as volatile, because we don't want any further compiler optimization
	// to remove them. 
	volatile_landmarks(M);

#if 0
	dbgs() << "Dumping module...\n";
	M.dump();
#endif
	check_dominance(M);
	verifyModule(M);

	// Calculate the number of original instructions left. 
	InstSet cloned_orig_insts;
	forallinst(M, ins) {
		++NumInstructionsInSliced;
		if (Instruction *orig_ins = clone_map_r.lookup(ins))
			cloned_orig_insts.insert(orig_ins);
	}
	NumOrigInstructionsLeft = cloned_orig_insts.size();
	
	return true;
}

void MaxSlicing::check_dominance(Module &M) {
	forallfunc(M, f) {
		if (f->isDeclaration())
			continue;
		DominatorTree &DT = getAnalysis<DominatorTree>(*f);
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				// LLVM has a bug with dominance check for InvokeInsts. 
				if (isa<InvokeInst>(ins))
					continue;
				for (Value::use_iterator ui = ins->use_begin(); ui != ins->use_end();
						++ui) {
					if (Instruction *user = dyn_cast<Instruction>(*ui)) {
						if (isa<PHINode>(user))
							continue;
						if (!DT.dominates(ins, user)) {
							errs() << "Does not dominate:\n";
							errs() << ins->getParent()->getParent()->getName() << "." <<
								ins->getParent()->getName() << ":" << *ins << "\n";
							errs() << user->getParent()->getParent()->getName() << "." <<
								user->getParent()->getName() << ":" << *user << "\n";
						}
						assert(DT.dominates(ins, user));
					}
				}
			}
		}
	}
}

void MaxSlicing::volatile_landmarks(Module &M) {
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();

	vector<int> thr_ids = LT.get_thr_ids();
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		int i = thr_ids[k];
		size_t n_trunks = LT.get_n_trunks(i);
		for (size_t j = 0; j < n_trunks; ++j) {
			if (LT.is_enforcing_landmark(i, j)) {
				DEBUG(dbgs() << "Volatiling Instruction " <<
						LT.get_landmark(i, j).ins_id << "\n";);
				assert(trace.count(i));
				assert(j < trace.find(i)->second.size());
				Instruction *old_inst = trace.find(i)->second[j];
				assert(clone_map.count(i));
				assert(j < clone_map.find(i)->second.size());
				Instruction *new_inst = clone_map.find(i)->second[j].lookup(old_inst);
				CallSite cs = CallSite::get(new_inst);
				assert(cs.getInstruction() &&
						"Enforcing landmarks must be CallInst/InvokeInst");
				// We claim that these enforcing landmarks may write to memory,
				// so that the optimizer will not remove them. 
				cs.setDoesNotAccessMemory(false);
				cs.setOnlyReadsMemory(false);
				// Marking the CallInst as volatile is not enough. 
				// We have to mark the function itself as volatile. 
				// Otherwise, LLVM would smartly tag those CallInst's back. 
				Function *callee = cs.getCalledFunction();
				if (callee) {
					callee->setDoesNotAccessMemory(false);
					callee->setOnlyReadsMemory(false);
				}
			}
		}
	}
}

char MaxSlicing::ID = 0;

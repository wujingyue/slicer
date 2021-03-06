/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "max-slicing"

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
using namespace llvm;

#include "rcs/FPCallGraph.h"
#include "rcs/IDManager.h"
#include "rcs/Exec.h"
using namespace rcs;

#include "slicer/landmark-trace.h"
#include "slicer/max-slicing.h"
using namespace slicer;

STATISTIC(NumPHINodesInserted, "Number of PHINodes inserted");

void MaxSlicing::redirect_program_entry(Instruction *old_start,
		Instruction *new_start) {
	// We simply swap the names of the old and new main. 
	Function *new_main = new_start->getParent()->getParent();
	Function *old_main = old_start->getParent()->getParent();
	StringRef old_main_name = old_main->getName();
	old_main->setName(old_main_name + OLDMAIN_SUFFIX);
	old_main->setLinkage(GlobalValue::InternalLinkage);
	new_main->setName(old_main_name);
}

void MaxSlicing::fix_def_use_phinodes(BasicBlock *bi) {
	if (!isa<PHINode>(bi->begin()))
		return;

	// Fix all PHINodes in this BB. 
	BBMapping actual_pred_bbs;
	const InstList &actual_pred_insts = cfg_r.lookup(bi->begin());
	for (size_t j = 0; j < actual_pred_insts.size(); ++j) {
		Instruction *actual_pred_inst = actual_pred_insts[j];
		EdgeType et = get_edge_type(actual_pred_inst, bi->begin());
		if (et == EDGE_INTRA_BB || et == EDGE_INTER_BB) {
			Instruction *orig_pred_inst = clone_map_r.lookup(actual_pred_inst);
			assert(orig_pred_inst);
			actual_pred_bbs[orig_pred_inst->getParent()] =
				actual_pred_inst->getParent();
		}
	}
	// main() {
	//   invoke foo(); normal = bb1, unwind = bb2
	//   bb1: I1
	// }
	//
	// foo() {
	//   I2
	//   ret
	// }
	//
	// cfg: I1 => ret
	// invoke_predecessor: I1 => invoke
	const InstList &invoke_pred_insts = invoke_predecessors.lookup(bi->begin());
	for (size_t j = 0; j < invoke_pred_insts.size(); ++j) {
		Instruction *orig_pred_inst = clone_map_r.lookup(invoke_pred_insts[j]);
		assert(orig_pred_inst);
		actual_pred_bbs[orig_pred_inst->getParent()] =
			invoke_pred_insts[j]->getParent();
	}

	for (BasicBlock::iterator ii = bi->begin();
			ii != (BasicBlock::iterator)bi->getFirstNonPHI(); ++ii) {
		PHINode *phi = dyn_cast<PHINode>(ii);
		// We may delete incoming values while traversing, therefore
		// we have to go backwards. 
		for (unsigned j = phi->getNumIncomingValues(); j > 0;) {
			--j;
			BasicBlock *incoming_bb = phi->getIncomingBlock(j);
			// If an incoming block does not appear in the cloned CFG, 
			// we remove this incoming value;
			// otherwise, we replace it with its counterpart in the
			// cloned CFG. 
			if (!actual_pred_bbs.count(incoming_bb))
				// Don't remove PHINodes even if it's empty. 
				// Dangerous because the clone mappings still have its
				// references. 
				phi->removeIncomingValue(j, false);
			else
				phi->setIncomingBlock(j, actual_pred_bbs[incoming_bb]);
		}
	}
}

void MaxSlicing::fix_def_use_terminator(BasicBlock *bb,
		BasicBlock *&unreachable_bb) {
	Function *f = bb->getParent();
	TerminatorInst *ti = bb->getTerminator();

	if (ti == NULL) {
		/*
		 * Some BBs don't have a Terminator because they end with functions
		 * such as pthread_exit() or exit(), or the CFG is not complete due
		 * to not exiting normally. 
		 * Need to append UnreachableInsts to them
		 * because LLVM requires all BBs to end with a TerminatorInst. 
		 */
		bool ends_with_no_return;
		if (bb->empty())
			ends_with_no_return = false;
		else {
			CallSite cs(&bb->back());
			if (!cs.getInstruction())
				ends_with_no_return = false;
			else
				ends_with_no_return = cs.doesNotReturn();
		}
		if (ends_with_no_return)
			new UnreachableInst(getGlobalContext(), bb);
		else {
			errs() << "[Warning] CFG broken due to incomplete execution: " <<
				f->getName() << "." << bb->getName() << "\n";
			if (!unreachable_bb)
				unreachable_bb = create_unreachable(f);
			BranchInst::Create(unreachable_bb, bb);
		}
	} else {
		BBMapping actual_succ_bbs; // Map to the cloned CFG. 
		InstList actual_succ_insts;
		if (invoke_successors.count(ti))
			actual_succ_insts = invoke_successors.lookup(ti);
		else
			actual_succ_insts = cfg.lookup(ti);

		for (size_t j = 0; j < actual_succ_insts.size(); ++j) {
			Instruction *orig_succ_inst =
				clone_map_r.lookup(actual_succ_insts[j]);
			assert(orig_succ_inst);
			assert(orig_succ_inst->getParent());
			actual_succ_bbs[orig_succ_inst->getParent()] =
				actual_succ_insts[j]->getParent();
		}
		for (unsigned j = 0; j < ti->getNumSuccessors(); ++j) {
			// If an outgoing block does not appear in the cloned CFG, 
			// we redirect the outgoing edge to the unreachable BB in
			// the function;
			// otherwise, redirect the outgoing edge to the cloned CFG. 
			BasicBlock *outgoing_bb = ti->getSuccessor(j);
			if (!actual_succ_bbs.count(outgoing_bb)) {
				if (!unreachable_bb) {
					// <f> does not have an unreachable BB yet. Create one. 
					// This statement changes <f> while we are still scanning
					// <f>, but it's fine because the BB list is simply a
					// linked list.
					unreachable_bb = create_unreachable(f);
				}
				ti->setSuccessor(j, unreachable_bb);
			} else {
				ti->setSuccessor(j, actual_succ_bbs[outgoing_bb]);
			}
		}
	} // if (ti == NULL)
}

void MaxSlicing::fix_def_use_bb(Module &M) {
	dbgs() << "Fixing BBs in def-use graph...\n";

	// An InvokeInst's successors may not be its successors in <cfg>. 
	// Need an extra pass of DFS to resolve them. 
	find_invoke_successors(M);

	DenseMap<Function *, BasicBlock *> unreach_bbs;
	forallfunc(M, fi) {
		// Skip original functions. 
		if (!clone_map_r.count(fi->begin()->begin()))
			continue;
		BasicBlock *unreachable_bb = NULL;
		forall(Function, bi, *fi) {
			assert(bi->begin() != bi->end());
			fix_def_use_phinodes(bi);
			// <unreachable_bb> may be changed inside this function, and the new
			// value will be used when processing the next BB. 
			fix_def_use_terminator(bi, unreachable_bb);
		} // for bb
	} // for func
	
	// Make sure each BB has only one terminator. 
	forallbb(M, bb) {
		unsigned n_terminators = 0;
		forall(BasicBlock, ins, *bb) {
			if (ins->isTerminator())
				n_terminators++;
		}
		if (n_terminators != 1) {
			errs() << bb->getParent()->getName() << "." << bb->getName() <<
				" has 0 or multiple terminators.\n";
		}
		assert(n_terminators == 1);
	}

	// Check the CFG. 
	forallbb(M, bb) {
		for (succ_iterator si = succ_begin(bb); si != succ_end(bb); ++si) {
			assert(bb->getParent() == (*si)->getParent());
		}
		for (pred_iterator pi = pred_begin(bb); pi != pred_end(bb); ++pi) {
			assert(bb->getParent() == (*pi)->getParent());
		}
	}
}

void MaxSlicing::fix_def_use(Module &M) {
	/*
	 * Things to fix:
	 * . BBs used in PHINodes and TerminatorInsts. 
	 * . Instructions. 
	 * . Function parameters
	 * . Functions used in call instructions. 
	 */
	// TODO: Each fix_def_use_ function iterates through all cloned
	// instructions for now. Could make it faster by maintaining a list
	// of unresolved operands. 
	dbgs() << "\nFixing def-use...\n";
	fix_def_use_bb(M);
	fix_def_use_insts(M);
	fix_def_use_func_param(M);
	fix_def_use_func_call(M);
	dbgs() << "Done fix_def_use\n";
}

void MaxSlicing::fix_def_use_func_call(Module &M) {

	dbgs() << "Fixing function calls in def-use graph...\n";
	
	forall(InstMapping, it, clone_map_r) {
		Instruction *ins = it->first;
		if (is_call(ins) && !is_intrinsic_call(ins)) {
			const InstList &next_insts = cfg.lookup(ins);
			Function *callee = NULL;
			for (size_t j = 0; j < next_insts.size(); ++j) {
				if (get_edge_type(ins, next_insts[j]) == EDGE_CALL) {
					// FIXME: does not handle function pointers with more than
					// one possible targets in the sliced program. 
					assert(callee == NULL && "For now, we allow only one target");
					callee = next_insts[j]->getParent()->getParent();
				}
			}
			// Sometimes, the call edges get removed by the refinement process. 
			// In that case, we simply let the instruction call the original
			// function; otherwise, we replace the target with <callee>. 
			if (callee) {
				CallSite cs(ins);
				if (callee->getType() == cs.getCalledValue()->getType()) {
					cs.setCalledFunction(callee);
				} else {
					// <callee> may not have the same signature as required by the
					// the call site, because our alias analysis catches bitcast.
					Value *wrapped_callee = new BitCastInst(
							callee, cs.getCalledValue()->getType(), "", ins);
					cs.setCalledFunction(wrapped_callee);
				}
			}
		}
	}
}

void MaxSlicing::fix_def_use_func_param(Module &M) {
	dbgs() << "Fixing function parameters in def-use graph...\n";

	forall(InstMapping, it, clone_map_r) {
		Instruction *ins = it->first;
		Instruction *orig_ins = it->second;
		Function *func = ins->getParent()->getParent();
		Function *orig_func = orig_ins->getParent()->getParent();
		ValueMapping params_map;
		Function::arg_iterator ai = func->arg_begin();
		Function::arg_iterator orig_ai = orig_func->arg_begin();
		for (; ai != func->arg_end() && orig_ai != orig_func->arg_end();
				++ai, ++orig_ai)
			params_map[orig_ai] = ai;
		assert(ai == func->arg_end() && orig_ai == orig_func->arg_end() &&
				"<func> and <orig_func> should have the same number of arguments");
		for (unsigned j = 0; j < ins->getNumOperands(); ++j) {
			Value *arg = params_map.lookup(ins->getOperand(j));
			if (arg)
				ins->setOperand(j, arg);
		}
	}
}

void MaxSlicing::fix_def_use_insts(Module &M) {
	dbgs() << "Fixing instructions in def-use graph...\n";

	forallfunc(M, f) {
		if (!f->isDeclaration())
			fix_def_use_insts(*f);
	}
}

void MaxSlicing::fix_def_use_insts(Function &F) {
	dbgs() << "Fixing instructions in Function " << F.getName() << "...\n";

	// An old instruction may correspond to multiple new instructions.
	DenseMap<Instruction *, InstSet> old_insts_to_new;
	DenseMap<Instruction *, vector<Use *> > uses_of_old_insts;
	
	// Collect all def-use chains in this function. 
	forall(Function, bb, F) {
		forall(BasicBlock, new_ins, *bb) {
			// Update <old_insts_to_new>. 
			if (Instruction *old_ins = clone_map_r.lookup(new_ins))
				old_insts_to_new[old_ins].insert(new_ins);
			// Update <uses_of_old_insts>.
			for (unsigned i = 0; i < new_ins->getNumOperands(); ++i) {
				Value *op = new_ins->getOperand(i);
				// Needn't consider ConstantExprs, because they are constant. 
				if (Instruction *used = dyn_cast<Instruction>(op)) {
					assert(!clone_map_r.count(used));
					uses_of_old_insts[used].push_back(&new_ins->getOperandUse(i));
				}
			}
		}
	}

	// Resolve them using SSAUpdater. 
	// TODO: SSAUpdater uses DFS to add PHINodes and resolve def-use chains. 
	// Therefore, it uses too much stack space sometimes. 
	for (DenseMap<Instruction *, InstSet>::iterator
			i = old_insts_to_new.begin(); i != old_insts_to_new.end(); ++i) {
		// old_ins => {new_ins, new_ins, ..., new_ins}
		Instruction *old_ins = i->first;
		SmallVector<PHINode *, 8> inserted_phis;
		SSAUpdater su(&inserted_phis);
		// <old_ins> is used as a prototype value. Only its name and type
		// are used by SSAUpdater. 
		// TODO: Interface changed in LLVM 2.9. 
		su.Initialize(old_ins->getType(), old_ins->getName());
		const InstSet &avail_values = i->second;
		forallconst(InstSet, j, avail_values) {
			Instruction *new_ins = *j;
			su.AddAvailableValue(new_ins->getParent(), new_ins);
		}
		DenseMap<Instruction *, vector<Use *> >::iterator k =
			uses_of_old_insts.find(old_ins);
		if (k != uses_of_old_insts.end()) {
			for (size_t j = 0; j < k->second.size(); ++j) {
				Use *use = k->second[j];
				Instruction *user = dyn_cast<Instruction>(use->getUser());
				assert(user && "The user of an instruction must be an instruction");
				// If there's a definition in the same BB and in front of <user>,
				// just use it. Otherwise, use SSAUpdater. 
				BasicBlock::iterator prev = user;
				bool found = false;
				while (prev != prev->getParent()->begin()) {
					--prev;
					if (avail_values.count(prev)) {
						found = true;
						break;
					}
				}
				if (found)
					use->set(prev);
				else
					su.RewriteUse(*k->second[j]);
			}
		}
		NumPHINodesInserted += inserted_phis.size();
	}
}

void MaxSlicing::link_thr_func(Module &M,
		int parent_tid, size_t trunk_id, int child_tid) {
	// <orig_site> is the pthread_create site in the original program. 
	Trace::const_iterator it = trace.find(parent_tid);
	assert(it != trace.end());
	Instruction *orig_site = it->second[trunk_id];
	
	// <new_site> is the pthread_create site in the cloned program. 
	assert(clone_map.count(parent_tid));
	Instruction *new_site = clone_map[parent_tid][trunk_id].lookup(orig_site);
	assert(new_site &&
			"Cannot find the thread creation site in the cloned CFG");
	
	// Old thread entry.
	Trace::const_iterator j = trace.find(child_tid);
	if (j == trace.end())
		errs() << "Cannot find thread ID " << child_tid << "\n";
	assert(j != trace.end());
	Instruction *orig_entry = j->second[0];
	
	// New thread entry.
	assert(clone_map.count(child_tid));
	// It's possible that the cloned thread function is not complete due to
	// not exiting normally. 
	// In that case, we still call the old thread function. 
	if (clone_map[child_tid].empty())
		return;
	Instruction *new_entry = clone_map[child_tid][0].lookup(orig_entry);
	assert(new_entry &&
			"Cannot find the thread entry in the cloned program.");

	// The thread function in the cloned program. 
	Function *thr_func = new_entry->getParent()->getParent();
	
	// Replace the target function in pthread_create to <thr_func>.
	assert(is_pthread_create(new_site));
	const Value *pth_create_callee = get_pthread_create_callee(new_site);
	if (pth_create_callee->getType() == thr_func->getType())
		set_pthread_create_callee(new_site, thr_func);
	else {
		// <thr_func> may not have the same signature as required by the
		// pthread_create, because our alias analysis catches bitcast.
		Value *wrapped_thr_func = new BitCastInst(
				thr_func, pth_create_callee->getType(), "", new_site);
		set_pthread_create_callee(new_site, wrapped_thr_func);
	}
}

/// Only used in function <link_thr_funcs>. 
struct ThreadCreationRecord {
	ThreadCreationRecord(int p, size_t t, int c):
		parent_tid(p), trunk_id(t), child_tid(c) {}
	int parent_tid;
	size_t trunk_id;
	int child_tid;
};

void MaxSlicing::link_thr_funcs(Module &M) {
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();

	// Collect all thread creation records. 
	vector<ThreadCreationRecord> thr_cr_records;
	const vector<int> &thr_ids = LT.get_thr_ids();
	for (size_t i = 0; i < thr_ids.size(); ++i) {
		int thr_id = thr_ids[i];
		for (unsigned j = 0; j < LT.get_n_trunks(thr_id); ++j) {
			const LandmarkTraceRecord &record = LT.get_landmark(i, j);
			if (record.child_tid != -1 && record.child_tid != thr_id) {
				thr_cr_records.push_back(
						ThreadCreationRecord(thr_id, j, record.child_tid));
			}
		}
	}

	for (size_t i = 0, E = thr_cr_records.size(); i < E; ++i) {
		link_thr_func(M,
				thr_cr_records[i].parent_tid,
				thr_cr_records[i].trunk_id,
				thr_cr_records[i].child_tid);
	}
}

bool MaxSlicing::is_unreachable(const BasicBlock *bb) {
	if (bb->getName().find("unreachable" + SLICER_SUFFIX) != string::npos)
		return true;

	// Don't always trust the name. Double check the content. 
	// Optimizations may combine BBs, and thus change the names. 
	if (!isa<UnreachableInst>(bb->getTerminator()))
		return false;
	
	forallconst(BasicBlock, ins, *bb) {
		if (const IntrinsicInst *intr = dyn_cast<IntrinsicInst>(ins)) {
			if (intr->getIntrinsicID() == Intrinsic::trap)
				return true;
		}
	}
	
	return false;
}

BasicBlock *MaxSlicing::create_unreachable(Function *f) {
	BasicBlock *unreachable_bb = BasicBlock::Create(
			f->getContext(), "unreachable" + SLICER_SUFFIX, f);
	// Insert an llvm.trap in the unreachable BB. 
	Function *trap = Intrinsic::getDeclaration(f->getParent(), Intrinsic::trap);
	CallInst::Create(trap, "", unreachable_bb);
	new UnreachableInst(getGlobalContext(), unreachable_bb);
	return unreachable_bb;
}

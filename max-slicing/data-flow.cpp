/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "max-slicing"

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/may-exec.h"
using namespace llvm;

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "max-slicing.h"
using namespace slicer;

void MaxSlicing::redirect_program_entry(
		Instruction *old_start,
		Instruction *new_start) {
	// We simply swap the names of the old and new main. 
	Function *new_main = new_start->getParent()->getParent();
	Function *old_main = old_start->getParent()->getParent();
	StringRef old_main_name = old_main->getName();
	old_main->setName(old_main_name + OLDMAIN_SUFFIX);
	old_main->setLinkage(GlobalValue::InternalLinkage);
	new_main->setName(old_main_name);
}

void MaxSlicing::fix_def_use_bb(Module &M) {

	dbgs() << "Fixing BBs in def-use graph...\n";
	DenseMap<Function *, BasicBlock *> unreach_bbs;
	forallfunc(M, fi) {
		forall(Function, bi, *fi) {
			assert(bi->begin() != bi->end());
			// Skip original BBs. 
			if (!clone_map_r.count(bi->begin()))
				continue;
			if (isa<PHINode>(bi->begin())) {
				// Fix all PHINodes in this BB. 
				BBMapping actual_pred_bbs;
				const InstList &actual_pred_insts = cfg_r.lookup(bi->begin());
				for (size_t j = 0; j < actual_pred_insts.size(); ++j) {
					Instruction *orig_pred_inst =
						clone_map_r.lookup(actual_pred_insts[j]);
					assert(orig_pred_inst);
					actual_pred_bbs[orig_pred_inst->getParent()] =
						actual_pred_insts[j]->getParent();
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
			// Fix the terminator. 
			TerminatorInst *ti = bi->getTerminator();
			if (ti == NULL) {
				/*
				 * Some BBs don't have a Terminator because they end with functions
				 * such as pthread_exit() or exit(), or the CFG is not complete due
				 * to not exiting normally. 
				 * Need to append UnreachableInsts to them
				 * because LLVM requires all BBs to end with a TerminatorInst. 
				 */
				bool ends_with_no_return;
				if (bi->empty())
					ends_with_no_return = false;
				else {
					CallSite cs = CallSite::get(&bi->back());
					if (!cs.getInstruction())
						ends_with_no_return = false;
					else
						ends_with_no_return = cs.doesNotReturn();
				}
				if (ends_with_no_return)
					new UnreachableInst(getGlobalContext(), bi);
				else {
					errs() << "[Warning] CFG broken due to incomplete execution: " <<
						fi->getNameStr() << "." << bi->getNameStr() << "\n";
					BasicBlock *&unreachable_bb = unreach_bbs[fi];
					if (!unreachable_bb)
						unreachable_bb = create_unreachable(fi);
					BranchInst::Create(unreachable_bb, bi);
				}
			} else {
				BBMapping actual_succ_bbs; // Map to the cloned CFG. 
				const InstList &actual_succ_insts = cfg.lookup(ti);
				for (size_t j = 0; j < actual_succ_insts.size(); ++j) {
					Instruction *orig_succ_inst =
						clone_map_r.lookup(actual_succ_insts[j]);
					assert(orig_succ_inst);
					actual_succ_bbs[orig_succ_inst->getParent()] =
						actual_succ_insts[j]->getParent();
				}
				for (unsigned j = 0; j < ti->getNumSuccessors(); ++j) {
					// If an outcoming block does not appear in the cloned CFG, 
					// we redirect the outcoming edge to the unreachable BB in
					// the function;
					// otherwise, redirect the outcoming edge to the cloned CFG. 
					BasicBlock *outcoming_bb = ti->getSuccessor(j);;
					if (!actual_succ_bbs.count(outcoming_bb)) {
						// NOTE: This refers to a slot in the hash map. 
						BasicBlock *&unreachable_bb = unreach_bbs[fi];
						if (!unreachable_bb) {
							// <fi> does not have an unreachable BB yet. Create one. 
							// This statement changes <fi> while we are still scanning
							// <fi>, but it's fine because the BB list is simply a
							// linked list.
							unreachable_bb = create_unreachable(fi);
						}
						ti->setSuccessor(j, unreachable_bb);
					} else {
						ti->setSuccessor(j, actual_succ_bbs[outcoming_bb]);
					}
				}
			} // if (ti == NULL)
		} // for bb
	} // for func
	
	forallbb(M, bi) {
		unsigned n_terminators = 0;
		forall(BasicBlock, ii, *bi) {
			if (ii->isTerminator())
				n_terminators++;
		}
		assert(n_terminators == 1);
	}
}

Instruction *MaxSlicing::find_op_in_cloned(
		Instruction *op,
		Instruction *user,
		const InstMapping &parent) {
	// <op'> must dominate <user>, therefore we trace back via <parent>.
	Instruction *op2 = user;
	do {
		Instruction *old_op2 = op2;
		op2 = parent.lookup(op2);
		assert(op2);
		assert(op2 != old_op2 && "Cannot find the corresponding operand");
		assert(clone_map_r.count(op2));
	} while (clone_map_r.lookup(op2) != op);
	return op2;
}

void MaxSlicing::fix_def_use(Module &M, const Trace &trace) {
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
	fix_def_use_insts(M, trace);
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
				// I guess this function works for both function pointers and
				// function definitions. 
				assert(callee->getType() == cs.getCalledValue()->getType());
				cs.setCalledFunction(callee);
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

void MaxSlicing::fix_def_use_insts(
		Module &M,
		const Trace &trace) {
	dbgs() << "Fixing instructions in def-use graph...\n";
	// Construct the DFS tree. 
	InstMapping parent;
	// We borrow assign_level to calculate <parent>. 
	// <level> is actually unused.
	DenseMap<Instruction *, int> level;
	forallconst(Trace, it, trace) {
		// There can be incomplete functions, due to not exiting normally. 
		// In that case, we skip this thread. 
		if (clone_map[it->first].empty())
			continue;
		Instruction *start = clone_map[it->first][0].lookup(it->second[0]);
		assert(start);
		parent[start] = start;
		assign_level(start, level, parent);
	}
	// In SSA, a definition dominates all its uses. Therefore, if we trace
	// back from a use via the DFS tree, we should always be able to find
	// its definition. 
	forall(InstMapping, it, clone_map_r) {
		Instruction *user = it->first;
		assert(user);
		if (!isa<PHINode>(user)) {
			for (unsigned j = 0; j < user->getNumOperands(); ++j) {
				Instruction *op = dyn_cast<Instruction>(user->getOperand(j));
				// Only fix instructions at this stage. 
				if (op == NULL)
					continue;
				// <op> is still in the original CFG.
				// Need to find its latest countepart <op'>. 
				user->setOperand(j, find_op_in_cloned(op, user, parent));
			}
		} else {
			// PHINode. Similar to non-PHINode case. 
			// Start tracing back from the tail of each incoming edge. 
			PHINode *phi = dyn_cast<PHINode>(user);
			for (unsigned j = 0; j < phi->getNumIncomingValues(); ++j) {
				Instruction *op = dyn_cast<Instruction>(phi->getIncomingValue(j));
				if (op == NULL)
					continue;
				Instruction *op2 = find_op_in_cloned(
						op,
						phi->getIncomingBlock(j)->getTerminator(),
						parent);
				phi->setIncomingValue(j, op2);
			}
		}
	}
}

void MaxSlicing::link_thr_func(
		Module &M,
		const Trace &trace,
		int parent_tid,
		size_t trunk_id,
		int child_tid) {
	// pthread_create in the original program. 
	Trace::const_iterator it = trace.find(parent_tid);
	assert(it != trace.end());
	Instruction *orig_site = it->second[trunk_id];
	// pthread_create in the cloned program. 
	assert(clone_map.count(parent_tid));
	Instruction *new_site = clone_map[parent_tid][trunk_id].lookup(orig_site);
	assert(new_site &&
			"Cannot find the thread creation site in the cloned CFG");
	// Old thread entry.
	Trace::const_iterator j = trace.find(child_tid);
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
	assert(is_call(new_site) && !is_intrinsic_call(new_site));
	CallSite cs(new_site);
	Function *callee = cs.getCalledFunction();
	assert(callee && (callee->getNameStr() == "pthread_create" ||
				callee->getNameStr() == "tern_wrap_pthread_create"));
	unsigned arg_no;
	if (callee->getNameStr() == "pthread_create") {
		// pthread_create(&t, NULL, foo, ...)	
		assert(cs.arg_size() >= 3 &&
				"A pthread_create must have at least 3 arguments.");
		arg_no = 2;
	} else {
		// tern_wrap_pthread_create(unknown, unknown, &t, NULL, foo, ...)
		assert(cs.arg_size() >= 5 &&
				"A tern_wrap_pthread_create must have at least 5 arguments.");
		arg_no = 4;
	}
	if (cs.getArgument(arg_no)->getType() == thr_func->getType())
		cs.setArgument(arg_no, thr_func);
	else {
		// <thr_func> may not have the same signature as required by the
		// pthread_create, because our alias analysis catches bitcast.
		Value *wrapped_thr_func = new BitCastInst(
				thr_func,
				cs.getArgument(arg_no)->getType(),
				"",
				new_site);
		cs.setArgument(arg_no, wrapped_thr_func);
	}
}

void MaxSlicing::link_thr_funcs(
		Module &M,
		const Trace &trace,
		const vector<ThreadCreationRecord> &thr_cr_records) {
	for (size_t i = 0, E = thr_cr_records.size(); i < E; ++i) {
		link_thr_func(M, trace,
				thr_cr_records[i].parent_tid,
				thr_cr_records[i].trunk_id,
				thr_cr_records[i].child_tid);
	}
}

bool MaxSlicing::is_unreachable(const BasicBlock *bb) {
	if (bb->getNameStr().find("unreachable" + SLICER_SUFFIX) != string::npos)
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

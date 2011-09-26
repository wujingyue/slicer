/**
 * Author: Jingyue
 */

#include "llvm/LLVMContext.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "common/IDManager.h"
#include "common/util.h"
using namespace llvm;

#include "slicer/trace.h"
#include "slicer/instrument.h"
#include "slicer/mark-landmarks.h"
using namespace slicer;

static RegisterPass<slicer::Instrument> X("instrument",
		"Instrument the program so that it will generate a trace"
		" when being executed");

static cl::opt<bool> InstrumentEachBB("instrument-each-bb",
		cl::desc("Instrument each BB so that we can get an almost full trace"));

static cl::opt<bool> MultiProcessed("multi-processed",
		cl::desc("Whether the program is multi-processed"));

const static char *BLOCKING_FUNCS[] = {
	"pthread_mutex_lock",
	"pthread_mutex_trylock",
	"pthread_join",
	"pthread_cond_wait",
	"pthread_cond_timedwait",
	"pthread_barrier_wait",
	"pthread_rwlock_wrlock",
	"pthread_rwlock_rdlock",
	"pthread_rwlock_trywrlock",
	"pthread_rwlock_tryrdlock",
	"sleep",
	"usleep",
	"nanosleep",
	"accept",
	"select",
	"sigwait",
	"sem_wait",
	"epoll_wait"
};

char Instrument::ID = 0;

void Instrument::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
	AU.addRequired<IDManager>();
	AU.addRequired<MarkLandmarks>();
	ModulePass::getAnalysisUsage(AU);
}

bool Instrument::blocks(Instruction *ins) {
	CallSite cs = CallSite::get(ins);
	if (!cs.getInstruction())
		return false;
	Function *callee = cs.getCalledFunction();
	if (!callee)
		return false;
	const size_t len = sizeof(BLOCKING_FUNCS) / sizeof(BLOCKING_FUNCS[0]);
	for (size_t i = 0; i < len; ++i) {
		if (callee->getNameStr() == BLOCKING_FUNCS[i])
			return true;
	}
	return false;
}

bool Instrument::should_instrument(Instruction *ins) const {
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();

	// Instruments landmarks (including derived ones).
	if (ML.is_landmark(ins))
		return true;

	// Instrument each BB if the flag is set. 
	if (InstrumentEachBB && ins == ins->getParent()->getFirstNonPHI())
		return true;
	if (InstrumentEachBB && is_call(ins))
		return true;
	if (InstrumentEachBB && is_ret(ins))
		return true;

	return false;
}

bool Instrument::runOnModule(Module &M) {
	IDManager &IDM = getAnalysis<IDManager>();

	setup(M);
	
	// Insert <trace_inst> for each instruction. 
	forallbb(M, bi) {
		for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
			if (CallInst *ci = dyn_cast<CallInst>(ii)) {
				if (ci->getCalledFunction() == trace_inst)
					continue;
			}

			if (!should_instrument(ii))
				continue;

			assert(!isa<PHINode>(ii) &&
					"PHINodes shouldn't be marked as landmarks.");
			unsigned ins_id = IDM.getInstructionID(ii);
			if (ins_id == IDManager::INVALID_ID)
				errs() << *ii << "\n";
			assert(ins_id != IDManager::INVALID_ID);
			// pthread_create needs a special wrapper. 
			// FIXME: Can be invoke pthread_create
			if (CallInst *ci = dyn_cast<CallInst>(ii)) {
				if (Function *callee = ci->getCalledFunction()) {
					if (callee->getNameStr() == "pthread_create") {
						assert(pth_create_wrapper &&
								"Cannot find the pthread_create wrapper");
						vector<Value *> args;
						args.push_back(ConstantInt::get(uint_type, ins_id));
						// Arguments start from index 1. 
						for (unsigned i = 1; i < ci->getNumOperands(); ++i)
							args.push_back(ci->getOperand(i));
						CallInst *new_ci = CallInst::Create(
								pth_create_wrapper, args.begin(), args.end());
						ReplaceInstWithInst(ci, new_ci);
						// Otherwise, ++ii will fail. 
						ii = new_ci;
						continue;
					}
				}
			}

			// Before the instruction if non-blocking. 
			// After the instruction if blocking. 
			if (!blocks(ii)) {
				CallInst::Create(trace_inst, ConstantInt::get(uint_type, ins_id),
						"", ii);
			} else {
				if (InvokeInst *inv = dyn_cast<InvokeInst>(ii)) {
					// TODO: We don't instrument the unwind BB currently. 
					BasicBlock *dest = inv->getNormalDest();
					assert(dest->getSinglePredecessor() == bi &&
							"Did you run nocrit?");
					CallInst::Create(
							trace_inst, ConstantInt::get(uint_type, ins_id), "",
							dest->getFirstNonPHI());
				} else {
					assert(bi->getTerminator() != ii &&
							"We assume terminators are non-blocking for now. "
							"Maynot be always true, e.g. invoke pthread_mutex_lock");
					/*
					 * inst 1
					 *   <== trace
					 * inst 2
					 */
					// ii -> inst 1
					++ii;
					// ii -> inst 2
					CallInst::Create(
							trace_inst, ConstantInt::get(uint_type, ins_id), "", ii);
					// ii -> inst 2
					--ii;
					// ii -> trace
				}
			}
		}
	}

	// Insert <init_trace> at the main entry. 
	forallfunc(M, f) {
		if (is_main(f)) {
			CallInst::Create(init_trace, ConstantInt::get(bool_type, MultiProcessed),
					"", f->begin()->begin());
		}
	}

	return true;
}

void Instrument::setup(Module &M) {
	// sizeof(unsigned) == 4
	uint_type = IntegerType::get(M.getContext(), 32);
	// sizeof(bool) = 1
	bool_type = IntegerType::get(M.getContext(), 8);

	FunctionType *trace_inst_fty = FunctionType::get(
			Type::getVoidTy(M.getContext()),
			vector<const Type *>(1, uint_type), false);
	FunctionType *init_trace_fty = FunctionType::get(
			Type::getVoidTy(M.getContext()),
			vector<const Type *>(1, bool_type), false);
	
	trace_inst = dyn_cast<Function>(
			M.getOrInsertFunction("trace_inst", trace_inst_fty));
	init_trace = dyn_cast<Function>(
			M.getOrInsertFunction("init_trace", init_trace_fty));
	if (Function *pth_create = M.getFunction("pthread_create")) {
		vector<const Type *> params;
		// ins_id
		params.push_back(uint_type);
		// old parameters of pthread_create
		const FunctionType *pth_create_type = pth_create->getFunctionType();
		for (unsigned i = 0; i < pth_create_type->getNumParams(); ++i)
			params.push_back(pth_create_type->getParamType(i));
		FunctionType *pth_create_wrapper_type = FunctionType::get(
				pth_create_type->getReturnType(),
				params,
				pth_create_type->isVarArg());
		pth_create_wrapper = dyn_cast<Function>(
				M.getOrInsertFunction("trace_pthread_create", pth_create_wrapper_type));
	} else {
		// Not every programs use pthread library. 
		pth_create_wrapper = NULL;
	}
}

#include "llvm/Pass.h"
#include "llvm/LLVMContext.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "idm/id.h"
using namespace llvm;

#include "trace.h"

namespace slicer {

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

	struct Instrument: public ModulePass {

		static char ID;

		Instrument(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

	private:
		void setup(Module &M);
		static bool blocks(Instruction *ins);

		const Type *uint_type;
		Function *trace, *pth_create_wrapper;
	};
	
	void Instrument::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<ObjectID>();
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

	bool Instrument::runOnModule(Module &M) {
		setup(M);
		forallbb(M, bi) {
			for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
				// Do not instrument PHI nodes because each BB must start with them. 
				if (isa<PHINode>(ii))
					continue;
				ObjectID &OI = getAnalysis<ObjectID>();
				unsigned ins_id = OI.getInstructionID(ii);
				// pthread_create needs a special wrapper. 
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
				if (!blocks(ii))
					CallInst::Create(trace, ConstantInt::get(uint_type, ins_id), "", ii);
				else {
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
					CallInst::Create(trace, ConstantInt::get(uint_type, ins_id), "", ii);
					// ii -> inst 2
					--ii;
					// ii -> trace
				}
			}
		}
		return true;
	}

	void Instrument::setup(Module &M) {
		// sizeof(unsigned) == 4
		uint_type = IntegerType::get(getGlobalContext(), 32);
		FunctionType *trace_type = FunctionType::get(
				Type::getVoidTy(getGlobalContext()),
				vector<const Type *>(1, uint_type),
				false);
		trace = dyn_cast<Function>(M.getOrInsertFunction("trace_inst", trace_type));
		Function *pth_create = M.getFunction("pthread_create");
		if (!pth_create) {
			pth_create_wrapper = NULL;
		} else {
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
					M.getOrInsertFunction(
						"trace_pthread_create", pth_create_wrapper_type));
		}
	}

	char Instrument::ID = 0;
}

namespace {
	
	static RegisterPass<slicer::Instrument> X(
			"instrument",
			"Instrument the program so that it will generate a trace"
			" when being executed",
			false,
			false);
}

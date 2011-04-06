#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "idm/id.h"
#include "common/include/util.h"
using namespace llvm;

#include "llvm-instrument/trace/landmark-trace.h"
#include "llvm-instrument/trace/trace-manager.h"
using namespace tern;

#include <fstream>
#include <sstream>
using namespace std;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "config.h"
#include "racy-pairs.h"

namespace {
	static RegisterPass<slicer::RacyPairs> X(
			"racy-pairs",
			"Dumps all racy pairs",
			false,
			true); // is analysis
	static cl::opt<int> SampleRate(
			"sample",
			cl::NotHidden,
			cl::desc("sample rate = 1 / sample"),
			cl::init(1));
}

namespace slicer {

	void RacyPairs::print_inst(Instruction *ins, raw_ostream &O) const {
		BasicBlock *bb = ins->getParent();
		Function *func = bb->getParent();
		O << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
		ins->print(O);
		O << "\n";
	}

	void RacyPairs::dump_inst(Instruction *ins) const {
		BasicBlock *bb = ins->getParent();
		Function *func = bb->getParent();
		cerr << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
		ins->dump();
	}

	void RacyPairs::print(raw_ostream &O, const Module *M) const {
		cerr << "# of racy pairs = " << racy_pairs.size() << endl;
		forallconst(vector<InstPair>, it, racy_pairs) {
			if ((it - racy_pairs.begin()) % 10000 == 0)
				cerr << "Progress: " << it - racy_pairs.begin()
					<< "/" << racy_pairs.size() << endl;
			O << string(10, '=') << "\n";
			print_inst(it->first, O);
			print_inst(it->second, O);
		}
	}

	void RacyPairs::select_load_store(
			unsigned s, unsigned e,
			int tid,
			vector<pair<Instruction *, CallStack> > &load_stores) {
		TraceManager &TM = getAnalysis<TraceManager>();
		for (unsigned p = s; p < e; ++p) {
			const TraceRecord &record = TM.get_record(p);
			if (record.thr_id != tid)
				continue;
			Instruction *ins = record.ins;
			if (!isa<LoadInst>(ins) && !isa<StoreInst>(ins))
				continue;
#ifdef CONTEXT
			AddCallingContext &ACC = getAnalysis<AddCallingContext>();
			load_stores.push_back(make_pair(ins, ACC.get_calling_context(p)));
#else
			load_stores.push_back(make_pair(ins, CallStack()));
#endif
		}
#ifdef VERBOSE
		cerr << "# of loads/stores = " << load_stores.size() << endl;
#endif
	}

	void RacyPairs::extract_racy_pairs(
			int t1, unsigned s1, unsigned e1,
			int t2, unsigned s2, unsigned e2) {

		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		
		// Select only load/store instructions. 
		vector<pair<Instruction *, CallStack> > b1, b2;
		select_load_store(s1, e1, t1, b1);
		select_load_store(s2, e2, t2, b2);
		// TODO: Can be optimized. a1 and a2 are ordered. 
		for (size_t i1 = 0; i1 < b1.size(); ++i1) {
			Instruction *ins1 = b1[i1].first;
			assert(isa<LoadInst>(ins1) || isa<StoreInst>(ins1));
			for (size_t i2 = 0; i2 < b2.size(); ++i2) {
				Instruction *ins2 = b2[i2].first;
				assert(isa<LoadInst>(ins2) || isa<StoreInst>(ins2));
				// If both are reads, not a race. 
				if (isa<LoadInst>(ins1) && isa<LoadInst>(ins2))
					continue;
				// Perform sampling
				++counter;
				if (counter < SampleRate)
					continue;
				counter = 0;
				// Actual query to bc2bdd
				Value *v1 = (isa<LoadInst>(ins1) ?
						dyn_cast<LoadInst>(ins1)->getPointerOperand() :
						dyn_cast<StoreInst>(ins1)->getPointerOperand());
				Value *v2 = (isa<LoadInst>(ins2) ?
						dyn_cast<LoadInst>(ins2)->getPointerOperand() :
						dyn_cast<StoreInst>(ins2)->getPointerOperand());
#ifdef CONTEXT
				vector<User *> ctxt1, ctxt2;
				ctxt1 = convert_context(b1[i1].second);
				ctxt2 = convert_context(b2[i2].second);
				print_context(b1[i1].second);
				dump_inst(ins1);
				print_context(b2[i2].second);
				dump_inst(ins2);
				if (BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0)) {
					racy_pairs.push_back(make_pair(ins1, ins2));
				}
#else
				if (BAA.alias(v1, 0, v2, 0))
					racy_pairs.push_back(make_pair(ins1, ins2));
#endif
			}
		}
	}

	void RacyPairs::print_context(const CallStack &cs) const {
		cerr << "Context:";
		for (size_t i = 0, E = cs.size(); i < E; ++i)
			cerr << ' ' << cs[i];
		cerr << endl;
	}

	vector<User *> RacyPairs::convert_context(const CallStack &cs) const {
		TraceManager &TM = getAnalysis<TraceManager>();
		vector<User *> res;
		for (size_t i = 0, E = cs.size(); i < E; ++i) {
			const TraceRecord &record = TM.get_record(cs[i]);
			res.push_back(record.ins);
		}
		return res;
	}

	size_t RacyPairs::find_next_enforce(
			const vector<unsigned> &indices,
			size_t j) const {
		size_t k = j + 1;
		while (k < indices.size()) {
			TraceManager &TM = getAnalysis<TraceManager>();
			const TraceRecord &record = TM.get_record(indices[k]);
			if (record.type == TR_LANDMARK_ENFORCE)
				break;
			++k;
		}
		return k;
	}

	bool RacyPairs::runOnModule(Module &M) {
#if 0
		ObjectID &IDM = getAnalysis<ObjectID>();
		vector<User *> ctxt1, ctxt2;
		ctxt1.push_back(IDM.getInstruction(6266));
		ctxt1.push_back(IDM.getInstruction(4917));
		ctxt1.push_back(IDM.getInstruction(3233));
		ctxt1.push_back(IDM.getInstruction(2996));
		ctxt1.push_back(IDM.getInstruction(361));
		Value *v1 = dyn_cast<StoreInst>(IDM.getInstruction(12))->getPointerOperand();
		ctxt2.push_back(IDM.getInstruction(6233));
		Value *v2 = dyn_cast<LoadInst>(IDM.getInstruction(4531))->getPointerOperand();
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		cerr << BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0) << endl;
#endif
#if 1
		// Calculate <sync_trunks> based on <trunks>.
		// A trunk may be bounded by non-enforcing landmarks. 
		// A sync trunk must be bounded by enforcing landmarks (except
		// the beginning and the end). 
		map<int, vector<unsigned> > sync_trunks;
		LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
		vector<int> thr_ids = LT.get_thr_ids();
		forall(vector<int>, it, thr_ids) {
			int thr_id = *it;
			const vector<unsigned> &indices = LT.get_thr_trunks(thr_id);
			size_t j = 0;
			while (j < indices.size()) {
				sync_trunks[thr_id].push_back(indices[j]);
				j = find_next_enforce(indices, j);
			}
			assert(sync_trunks[thr_id].size() > 0);
			if (sync_trunks[thr_id].back() < indices.back())
				sync_trunks[thr_id].push_back(indices.back());
		}
		// Collect racy pairs. 
		// Scan sync_trunks instead of trunks. 
		racy_pairs.clear();
		map<int, vector<unsigned> >::iterator i1, i2;
		for (i1 = sync_trunks.begin(); i1 != sync_trunks.end(); ++i1) {
			int t1 = i1->first;
			for (i2 = i1, ++i2; i2 != sync_trunks.end(); ++i2) {
				int t2 = i2->first;
				const vector<unsigned> &a1 = i1->second, &a2 = i2->second;
				// j1 and j2 are trunk IDs. 
				size_t j1 = 0;
				size_t j2 = 0;
				while (j1 < a1.size() && j2 < a2.size()) {
					// Thread t1 Trunk j1: [s1, e1)
					// Thread t2 Trunk j2: [s2, e2)
					// If the last trunk which has only one instruction, 
					// we virtually set e1 = s1 + 1 and e2 = s2 + 1.
					unsigned s1 = a1[j1];
					unsigned s2 = a2[j2];
					unsigned e1 = (j1 + 1 < a1.size() ? a1[j1 + 1] : s1 + 1);
					unsigned e2 = (j2 + 1 < a2.size() ? a2[j2 + 1] : s2 + 1);
					if (e1 > s2 && e2 > s1) {
#ifdef VERBOSE
						cerr << "Concurrent trunks: ";
						cerr << t1 << ":[" << s1 << ", " << e1 << ") "
							<< t2 << ":[" << s2 << ", " << e2 << ")" << endl;
#endif
						extract_racy_pairs(t1, s1, e1, t2, s2, e2);
					}
					if (j1 + 1 >= a1.size()) {
						++j2;
					} else if (j2 + 1 >= a2.size()) {
						++j1;
					} else {
						assert(j1 + 1 < a1.size() && j2 + 1 < a2.size());
						if (a1[j1 + 1] < a2[j2 + 1])
							++j1;
						else
							++j2;
					}
				}
			}
		}
#endif
		return false;
	}

	void RacyPairs::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<BddAliasAnalysis>();
		AU.addRequired<LandmarkTrace>();
		AU.addRequired<TraceManager>();
#ifdef CONTEXT
		AU.addRequired<AddCallingContext>();
#endif
		ModulePass::getAnalysisUsage(AU);
	}

	char RacyPairs::ID = 0;
}


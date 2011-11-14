#include <fstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "llvm/Support/CommandLine.h"
#include "common/IDManager.h"
#include "common/IDAssigner.h"
using namespace llvm;

#include "slicer/query-translator.h"
#include "slicer/clone-info-manager.h"
using namespace slicer;

static RegisterPass<QueryTranslator> X("translate-queries",
		"Translate queries on the original program to "
		"those on the sliced program",
		false, true);

static cl::opt<string> RawQueryList("input-raw-queries",
		cl::desc("The path to the input file containing raw queries"));
static cl::opt<string> QueryList("output-queries",
		cl::desc("The path to the output file containing queries"));

char QueryTranslator::ID = 0;

bool QueryTranslator::parse_raw_query(const string &line,
		vector<DynamicInsID> &a, vector<DynamicInsID> &b) {
	if (!regex_match(line, regex("[\\d\\s(),]*$")))
		return false;

	a.clear(); b.clear();
	bool second = false;
	size_t i = 0;
	while (true) {
		while (i < line.length() && line[i] != '(') {
			if (line[i] == ',')
				second = true;
			++i;
		}
		if (i >= line.length())
			break;
		size_t j = i + 1;
		while (j < line.length() && line[j] != ')')
			++j;
		assert(j < line.length() && "Wrong format");
		smatch what;
		// (12, 23, 34)
		assert(regex_match(line.substr(i, j - i + 1), what,
					regex("\\((\\d+),\\s+(\\d+),\\s+(\\d+)\\)")));
		DynamicInsID di;
		di.thread_id = atoi(what[1].str().c_str());
		di.trunk_id = atol(what[2].str().c_str());
		di.ins_id = atol(what[3].str().c_str());
		(second ? b : a).push_back(di);
		i = j + 1;
	}

	return true;
}

void QueryTranslator::print_contexted_ins(ostream &O,
		const vector<unsigned> &a) {
	for (size_t i = 0; i < a.size(); ++i) {
		O << a[i];
		if (i + 1 < a.size())
			O << " ";
	}
}

void QueryTranslator::print_query(ostream &O,
		const vector<unsigned> &a, const vector<unsigned> &b) {
	if (!a.empty() && !b.empty()) {
		print_contexted_ins(O, a);
		O << ", ";
		print_contexted_ins(O, b);
	}
}

void QueryTranslator::translate_contexted_ins(const vector<DynamicInsID> &a,
		vector<unsigned> &a3) {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	IDManager &IDM = getAnalysis<IDManager>();
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	InstList a2;
	for (size_t i = a.size(); i > 0; ) {
		--i;
		InstList cloned_insts = CIM.get_instructions(
				a[i].thread_id, a[i].trunk_id, a[i].ins_id);
		if (cloned_insts.size() >= 2) {
			errs() << "[Warning] Found two cloned instructions. Pick the first one.\n";
			for (size_t j = 0; j < cloned_insts.size(); ++j)
				errs() << *cloned_insts[j] << "\n";
		}
		bool skipped = true;
		if (cloned_insts.size() > 0) {
			a2.push_back(cloned_insts[0]);
			skipped = false;
		} else {
			if (Instruction *cloned_ins = IDM.getInstruction(a[i].ins_id)) {
				CallSite cs(cloned_ins);
				if (!cs.getInstruction() || !cs.getCalledFunction()) {
					// If not a call or calls a function pointer, it wouldn't be inlined. 
					a2.push_back(cloned_ins);
					skipped = false;
				} else {
					assert(cs.getInstruction() && cs.getCalledFunction());
					if (!a2.empty() &&
							a2.back()->getParent()->getParent() == cs.getCalledFunction()) {
						a2.push_back(cloned_ins);
						skipped = false;
					}
				}
			}
		}
		// If cannot find the counterpart of the instruction
		// (excluding the call stack), we simply give up. 
		if (i + 1 == a.size() && skipped) {
			a2.push_back(NULL);
			break;
		}
	}

	a3.clear();
	for (size_t i = 0; i < a2.size(); ++i)
		a3.push_back(IDA.getInstructionID(a2[i]));
	reverse(a3.begin(), a3.end());
}

bool QueryTranslator::runOnModule(Module &M) {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	assert(CIM.has_clone_info() && "This pass can be applied to the sliced/"
			"simplified program only");

	ifstream fin(RawQueryList.c_str());
	ofstream fout(QueryList.c_str());
	string line;
	while (getline(fin, line)) {
		vector<DynamicInsID> a, b;
		if (parse_raw_query(line, a, b)) {
			vector<unsigned> a2, b2;
			translate_contexted_ins(a, a2);
			translate_contexted_ins(b, b2);
			print_query(fout, a2, b2);
			fout << "\n";
		}
	}

	return false;
}

void QueryTranslator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDManager>();
	AU.addRequired<IDAssigner>();
	AU.addRequired<CloneInfoManager>();
	ModulePass::getAnalysisUsage(AU);
}

/**
 * Author: Jingyue
 */

#include <boost/regex.hpp>
#include <iostream>

using namespace std;
using namespace boost;

#define CLONED (0)

struct InstRecord {
	InstRecord(const string &f, const string &b, const string &i)
		: func(f), bb(b), ins(i) {}
	string func;
	string bb;
	string ins;
};

bool starts_with(const string &a, const string &b) {
	return a.compare(0, b.size(), b) == 0;
}

int main(int argc, char *argv[]) {
	string line;
	while (getline(cin, line)) {
		if (line.find(string(10, '=')) != string::npos) {
			// The next two lines indicate an alias pair. 
			regex re_ins("([^:]+):([^:]+):\\t(.+)");
			vector<InstRecord> recs;
			for (int i = 0; i < 2; ++i) {
				getline(cin, line);
				match_results<string::const_iterator> what_ins;
				assert(regex_match(line, what_ins, re_ins));
				recs.push_back(InstRecord(what_ins[1], what_ins[2], what_ins[3]));
			}
			assert(recs.size() == 2);
#if CLONED == 0
			if (!starts_with(recs[0].func, "_Z19consumer_decompressPv"))
				continue;
#else
			if (!starts_with(recs[0].func, "_Z19consumer_decompressPv.SLICER"))
				continue;
			if (!starts_with(recs[1].func, "_Z19consumer_decompressPv.SLICER"))
				continue;
			if (recs[0].func >= recs[1].func)
				continue;
#endif
			cout << string(10, '=') << endl;
			for (int i = 0; i < 2; ++i) {
				cout << recs[i].func << ":" << recs[i].bb << ":\t"
					<< recs[i].ins << endl;
			}
		}
	}
}


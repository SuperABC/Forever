#include "mod_test.h"

#include <cstdio>
#include <cstring>
#include <string>


using namespace std;

bool CheckModIdentity(const char* conceptName, const char* type, const char* name, TestPrintFn print) {
	auto say = [&](const string& text) { if (print) print(text.c_str()); };
	bool ok = true;
	if (!type || !*type) {
		say(string("[") + conceptName + "] GetType() is empty");
		return false;
	}
	const string prefix = string(conceptName) + "_";
	if (strncmp(type, prefix.c_str(), prefix.size()) != 0) {
		say(string("[") + conceptName + "] type '" + type + "' does not start with '" + prefix + "'");
		ok = false;
	}
	if (!name || !*name) {
		say(string("[") + conceptName + "] mod '" + type + "' has an empty GetName()");
		ok = false;
	}
	if (ok) say(string(type) + " name=" + name);
	return ok;
}

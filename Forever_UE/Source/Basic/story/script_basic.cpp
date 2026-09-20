#include "script_basic.h"

using namespace std;

int ScriptBasic::count = 0;

ScriptBasic::ScriptBasic() : id(count++) {
}

const char* ScriptBasic::GetName() {
	name = "ScriptBasic" + to_string(id);
	return name.data();
}

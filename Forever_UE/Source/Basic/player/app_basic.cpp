#include "app_basic.h"

using namespace std;

int AppBasic::count = 0;

AppBasic::AppBasic() : id(count++) {
}

const char* AppBasic::GetName() {
	name = "AppBasic" + to_string(id);
	return name.data();
}

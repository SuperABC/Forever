#include "manufacture_basic.h"

using namespace std;

int ManufactureBasic::count = 0;

ManufactureBasic::ManufactureBasic() : id(count++) {
}

const char* ManufactureBasic::GetName() {
	name = "ManufactureBasic" + to_string(id);
	return name.data();
}

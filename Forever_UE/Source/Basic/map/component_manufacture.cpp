#include "component_manufacture.h"

using namespace std;

int ManufactureComponent::count = 0;

ManufactureComponent::ManufactureComponent() : id(count++) {
}

const char* ManufactureComponent::GetName() {
	name = "ManufactureComponent" + to_string(id);
	return name.data();
}

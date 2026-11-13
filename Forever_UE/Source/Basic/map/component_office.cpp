#include "component_office.h"

using namespace std;

int OfficeComponent::count = 0;

OfficeComponent::OfficeComponent() : id(count++) {
}

const char* OfficeComponent::GetName() {
	name = "OfficeComponent" + to_string(id);
	return name.data();
}

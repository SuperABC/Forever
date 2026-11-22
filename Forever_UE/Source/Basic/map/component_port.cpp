#include "component_port.h"

using namespace std;

int PortComponent::count = 0;

PortComponent::PortComponent() : id(count++) {
}

const char* PortComponent::GetName() {
	name = "PortComponent" + to_string(id);
	return name.data();
}

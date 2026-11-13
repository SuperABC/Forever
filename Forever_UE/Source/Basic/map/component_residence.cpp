#include "component_residence.h"

using namespace std;

int ResidenceComponent::count = 0;

ResidenceComponent::ResidenceComponent() : id(count++) {
}

const char* ResidenceComponent::GetName() {
	name = "ResidenceComponent" + to_string(id);
	return name.data();
}

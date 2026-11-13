#include "component_airport.h"

using namespace std;

int AirportComponent::count = 0;

AirportComponent::AirportComponent() : id(count++) {
}

const char* AirportComponent::GetName() {
	name = "AirportComponent" + to_string(id);
	return name.data();
}

#include "component_train_station.h"

using namespace std;

int TrainStationComponent::count = 0;

TrainStationComponent::TrainStationComponent() : id(count++) {
}

const char* TrainStationComponent::GetName() {
	name = "TrainStationComponent" + to_string(id);
	return name.data();
}

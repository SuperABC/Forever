#include "room_train_station.h"

using namespace std;

int TrainStationRoom::count = 0;

TrainStationRoom::TrainStationRoom() : id(count++) {
}

const char* TrainStationRoom::GetName() {
	name = "TrainStationRoom" + to_string(id);
	return name.data();
}

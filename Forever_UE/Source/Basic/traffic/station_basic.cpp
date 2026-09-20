#include "station_basic.h"

using namespace std;

int StationBasic::count = 0;

StationBasic::StationBasic() : id(count++) {
}

const char* StationBasic::GetName() {
	name = "StationBasic" + to_string(id);
	return name.data();
}

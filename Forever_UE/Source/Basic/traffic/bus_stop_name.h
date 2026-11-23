#pragma once

#include <string>

// 手动公交站(编辑器"路网路线")的站名：线下标/站下标是TransitData::roadLines里的全局下标。
// station_bus(摆站)和route_bus(连线)必须用同一个名字，Core靠站名把线和站对上，所以放在这个共用头里。
inline std::string BusStopName(int lineIndex, int stopIndex) {
	return "BusStop_L" + std::to_string(lineIndex) + "_S" + std::to_string(stopIndex);
}

#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Traffic;

#define SAVE_SECTION_TRAFFIC "world.traffic"

// 交通域的存档读写：站点(三类：建筑站点/贴路站点/边缘站点)、线路(站点表、连线、各条线路的
// 停靠顺序、每条线路上的车)、车辆(类型、停车位、线路绑定、位置、选项)和公交计时。
// 线路的连线几何(RouteEdge)不存：地图按存档原样恢复，读档时用Route::BuildEdgeGeometry按
// 同样的站点接口重新算，结果一致。详见traffic_save.md。
class TrafficSaveCodec {
public:
	static void Save(const Traffic& traffic, const Map& map, SaveArchive& archive);

	// 按存档重建Traffic(map必须已经按同一份存档恢复)。restoreOptions为true时恢复车辆的
	// 交互选项(读档跳过game_start广播时，见script_save.md)。存档里没有交通段时返回nullptr、
	// error为空；段损坏时返回nullptr并给出原因。
	static Traffic* Load(const SaveArchive& archive, Map& map, bool restoreOptions, std::string& error);
};
